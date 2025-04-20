<script setup>
import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import router from "./router";
import { useRoute } from "vue-router";
import { ref, onMounted, provide } from "vue";
import { Button } from "@/components/ui/button";
import { WifiOff, Bell, Loader2 } from "lucide-vue-next";

import {
  AlertDialog,
  AlertDialogAction,
  AlertDialogCancel,
  AlertDialogContent,
  AlertDialogDescription,
  AlertDialogFooter,
  AlertDialogHeader,
  AlertDialogTitle,
  AlertDialogTrigger,
} from "@/components/ui/alert-dialog";

import {
  Dialog,
  DialogClose,
  DialogContent,
  DialogDescription,
  DialogFooter,
  DialogHeader,
  DialogTitle,
  DialogTrigger,
} from "@/components/ui/dialog";

const show_reconnect_btn = ref(false);
const connection_lost_noti = ref(false);
const notification = ref(false);
const notification_msg = ref("");
const error = ref(false);
const error_msg = ref("");
const serverAddress = ref("");
const serverPort = ref();
const deviceName = ref("");
const send_cb = ref(false);
const recv_cb = ref(true);
provide("serverAddress", serverAddress);
provide("serverPort", serverPort);
provide("deviceName", deviceName);
provide("SendCB", send_cb);
provide("RecvCB", recv_cb);

async function clipboard_send_and_receive(receive_content, send_content) {
  if (receive_content == false) {
    recv_cb.value = false;
    await invoke("stop_clipboard_receive", {});
  }
  if (send_content == true) {
    await invoke("start_clipboard_monitor", {});
    send_cb.value = true;
  }
}

function adaptiveBackoff(
  attempt,
  baseDelay = 5000,
  maxDelay = 300000,
  threshold = 5,
) {
  // Linear increase for first 'threshold' attempts, then exponential growth
  let waitTime =
    attempt <= threshold
      ? baseDelay + (attempt - 1) * 1000
      : baseDelay * Math.pow(1.3, attempt - threshold);

  return Math.min(waitTime, maxDelay);
}

function sleep(ms) {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

// check if client has persistent connection with the server
async function isConnected() {
  try {
    await invoke("isconnected", {});
    show_reconnect_btn.value = false;
    connection_lost_noti.value = false;
    return true;
  } catch (error) {
    return false;
  }
}
async function loop() {
  let threshold = 30;
  let attempt = 1;

  while (true) {
    await invoke("try_reconnect");
    if (await isConnected()) break;
    await sleep(adaptiveBackoff(attempt, undefined, undefined, threshold));
    attempt++;
  }
  console.log("breaking out to retry loop!!!");
}

const route = useRoute();

async function manual_reconnect() {
  await invoke("try_reconnect");
  await isConnected();
}

async function setupListener() {
  await listen("disconnect", async (event) => {
    show_reconnect_btn.value = true;
    connection_lost_noti.value = true;
    loop();
  });
}

listen("login_success", async (event) => {
  await invoke("group_check");
});

listen("verify-access-status", async (event) => {
  if (1 == Number(event.payload)) {
    await invoke("register_device", {
      deviceName: deviceName.value,
    });
  } else if (0 == Number(event.payload)) {
    waiting.value = false;
  }
});

listen("cb-state", async (event) => {
  const payload = event.payload;

  console.log("The payload -> ", payload);
  await clipboard_send_and_receive(payload.recv_cb, payload.send_cb);
});

listen("notification", (event) => {
  notification.value = true;
  notification_msg.value = event.payload;
});

listen("error", (event) => {
  error.value = true;
  error_msg.value = event.payload;
});

listen("show-page", (event) => {
  console.log("RECEIVED EVENVT: show-page", event.payload);
  if (event.payload == "new") {
    router.push({ path: "/server_connect" });
  } else if (event.payload == "group_join") {
    router.push({ path: "/group_join" });
  } else if (event.payload == "home") {
    router.push({ path: "/home" });
  } else if (event.payload == "login_failed") {
    router.push({ path: "/login_failed" });
  } else {
    console.log("unknown palyload to show page", event.payload);
  }
});
const recon_spin = ref(false);
const startLoading = () => {
  recon_spin.value = true;
  setTimeout(() => {
    recon_spin.value = false;
  }, 1000);
};

onMounted(async () => {
  setupListener();
  if (!(await isConnected())) {
    loop();
  }
});
</script>
<template>
  <div class="notifications">
    <Dialog v-model:open="notification">
      <DialogContent class="sm:max-w-md">
        <DialogHeader>
          <DialogTitle>
            <div class="flex gap-2 font-light"><Bell />Notification</div>
          </DialogTitle>
          <DialogDescription>
            {{ notification_msg }}
          </DialogDescription>
        </DialogHeader>
      </DialogContent>
    </Dialog>
    <Dialog v-model:open="error">
      <DialogContent class="sm:max-w-md">
        <DialogHeader>
          <DialogTitle class="text-red-500">Error</DialogTitle>
          <DialogDescription>
            {{ error_msg }}
          </DialogDescription>
        </DialogHeader>
      </DialogContent>
    </Dialog>
  </div>
  <main class="container">
    <RouterView />
    <div
      v-if="show_reconnect_btn"
      class="absolute bottom-2 left-1/2 transform -translate-x-1/2"
    >
      <Button
        variant="outline"
        class="h-6 font-light px-3"
        @click="
          manual_reconnect();
          startLoading();
        "
        >Reconnect
        <Loader2 v-if="recon_spin" class="animate-spin" />
      </Button>
    </div>
  </main>

  <AlertDialog v-model:open="connection_lost_noti">
    <AlertDialogContent>
      <AlertDialogHeader>
        <AlertDialogTitle>
          <div class="flex gap-2 font-light">
            <WifiOff /> Server Connection Lost
          </div>
        </AlertDialogTitle>
      </AlertDialogHeader>

      <AlertDialogFooter>
        <AlertDialogCancel>Ok</AlertDialogCancel>
      </AlertDialogFooter>
    </AlertDialogContent>
  </AlertDialog>
</template>

<style>
.global-actions {
  position: absolute;
  bottom: 10px;
  left: 50%;
  transform: translateX(-50%);
}
.try-recon {
  button {
    color: white;
    font-weight: 300;
    background: transparent;
    border: none;
    padding: 0;
    cursor: pointer;
    border: 1px solid white;
    padding: 0 10px;
  }
}
</style>
