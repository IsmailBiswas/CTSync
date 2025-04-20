<script setup>
import { writeText, readText } from "@tauri-apps/plugin-clipboard-manager";
import DarkMode from "../components/DarkMode.vue";

import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import { Switch } from "@/components/ui/switch";
import { Input } from "@/components/ui/input";
import { Settings, ClipboardCopy, Trash2 } from "lucide-vue-next";

import ConnectedDevices from "../components/ConnectedDevices.vue";
import { ref, onMounted, inject } from "vue";
const group_invite_key = ref("No key");
const req_device_name = ref("");
const req_device_hash = ref("");
const req_device_id = ref("");
const join_req = ref(false);
const invite_key_expire = ref(1);
const devices = ref([]);
const send_cb = inject("SendCB");
const recv_cb = inject("RecvCB");
import { Label } from "@/components/ui/label";

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
  NumberField,
  NumberFieldContent,
  NumberFieldDecrement,
  NumberFieldIncrement,
  NumberFieldInput,
} from "@/components/ui/number-field";
import {
  Sheet,
  SheetClose,
  SheetContent,
  SheetDescription,
  SheetFooter,
  SheetHeader,
  SheetTitle,
  SheetTrigger,
} from "@/components/ui/sheet";
import { Button } from "@/components/ui/button";
import { Icon } from "@iconify/vue";

// Pass { disableTransition: false } to enable transitions

const clickedCopyInviteKey = ref(false);

function copyInviteKeyHandle() {
  navigator.clipboard.writeText(group_invite_key.value);

  clickedCopyInviteKey.value = true;
  setTimeout(() => {
    clickedCopyInviteKey.value = false;
  }, 300);
}


async function start_cb_monitor() {
  await invoke("start_clipboard_monitor", {});
  send_cb.value = true;
}
async function stop_cb_monitor() {
  await invoke("stop_clipboard_monitor", {});
  send_cb.value = false;
}


async function leave_group() {
  await invoke("leave_group", {});
}

async function accept_join() {
  if (req_device_id.value) {
    await invoke("accept_device", {
      deviceId: req_device_id.value,
    });
  }
}
async function generate_invite_key() {
  await invoke("generate_invite_key", {
    expHour: invite_key_expire.value,
  });
}
async function get_invite_key() {
  await invoke("get_invite_key", {});
}
async function get_online_devices() {
  console.log("Getting online devices list...");
  await invoke("get_online_devices", {});
}
listen("show_invite_key", (event) => {
  group_invite_key.value = event.payload;
});
listen("group_join_request", (event) => {
  join_req.value = true;
  const payload = event.payload;
  req_device_name.value = payload.device_alias;
  req_device_hash.value = payload.pub_key_hash;
  req_device_id.value = payload.device_id;
});

listen("online_devices", (event) => {
  const jsonObject = JSON.parse(event.payload);
  devices.value = jsonObject;
});

async function start_clipboard_recv() {
  await invoke("start_clipboard_receive", {});
  recv_cb.value = true;
}
async function stop_clipboard_recv() {
  await invoke("stop_clipboard_receive", {});
  recv_cb.value = false;
}

async function toggle_cb_recv() {
  if (recv_cb.value) {
    await stop_clipboard_recv();
  } else {
    await start_clipboard_recv();
  }
}

async function toggle_cbt_send() {
  if (send_cb.value) {
    await stop_cb_monitor();
  } else {
    await start_cb_monitor();
  }
}
async function restore_state(){}

onMounted(async () => {
  await get_online_devices();
  await get_invite_key();
  await restore_state()

});
</script>

<template>
  <div class="my-4 flex justify-between">
    <DarkMode />
    <Sheet>
      <SheetTrigger as-child>
        <Button variant="outline" size="icon" class="w-8 h-8">
          <Settings />
        </Button>
      </SheetTrigger>
      <SheetContent>
        <SheetHeader>
          <SheetTitle>Settings</SheetTitle>
          <SheetDescription> </SheetDescription>
        </SheetHeader>
        <div>
          <div class="border p-2 flex flex-col gap-2 rounded-md">
            <div>
              <p class="flex justify-center text-xs font-light text-stone-400">
                Group Key
              </p>
            </div>
            <div class="flex items-center gap-4">
              <p class="break-words font-light text-sm">
                <span class="">
                  {{ group_invite_key }}
                </span>
              </p>
              <div class="flex gap-2">
                <ClipboardCopy
                  @click="copyInviteKeyHandle"
                  :size="15"
                  :class="[
                    'transition-colors duration-300',
                    clickedCopyInviteKey ? 'text-emerald-600' : 'text-current',
                    clickedCopyInviteKey ? 'scale-125' : 'scale-100',
                  ]"
                />
              </div>
            </div>
            <div class="sm:flex gap-2">
              <Button
                :class="{
                  btnDisabled: !invite_key_expire || invite_key_expire <= 0,
                }"
                @click="generate_invite_key()"
                variant="outline"
                class="h-6 p-2 mb-1 sm:my-0 text-xs font-light"
              >
                Create Key
              </Button>
              <div class="exp-div flex gap-2 items-center">
                <span class="font-light text-xs">Exp in: </span>
                <NumberField
                  v-model="invite_key_expire"
                  id="age"
                  :default-value="1"
                  :min="0"
                >
                  <div class="w-12">
                    <NumberFieldContent>
                      <NumberFieldInput
                        class="h-5 px-2 mx-0 text-xs font-light"
                      />
                    </NumberFieldContent>
                  </div>
                </NumberField>
                <div class="flex font-light text-xs items-center gap-2">
                  <span v-if="invite_key_expire < 2">Hour</span>
                  <span v-if="invite_key_expire > 1">Hours</span>
                </div>
              </div>
            </div>
          </div>
        </div>

        <SheetFooter>

          <SheetClose as-child> </SheetClose>
        </SheetFooter>
      </SheetContent>
    </Sheet>
  </div>

  <div class="home-container w-full">
    <AlertDialog v-model:open="join_req">
      <AlertDialogContent>
        <AlertDialogHeader>
          <AlertDialogTitle>Group join request</AlertDialogTitle>
          <AlertDialogDescription></AlertDialogDescription>
        </AlertDialogHeader>

        <div class="space-y-2">
          <p class="flex flex-wrap gap-2 font-thin">
            <span class="label">Device name:</span>
            <span class="break-all">{{ req_device_name }}</span>
          </p>

          <p class="flex gap-2 font-thin">
            <span class="label">Hash:</span>
            <span class="break-all">{{ req_device_hash }}</span>
          </p>
        </div>

        <AlertDialogFooter>
          <AlertDialogCancel>Cancel</AlertDialogCancel>
          <AlertDialogAction @click="accept_join()">
            Approve
          </AlertDialogAction>
        </AlertDialogFooter>
      </AlertDialogContent>
    </AlertDialog>

    <div class="main-actions max-w-[310px]">
      <div
        class="flex flex-row items-center justify-between rounded-lg border p-4 shadow"
      >
        <div class="space-y-0.5">
          <span class="text-base">Send clipboard text</span>
        </div>
        <Switch :checked="send_cb" @update:checked="toggle_cbt_send()" />
      </div>
      <div
        class="flex flex-row items-center justify-between rounded-lg border p-4 shadow"
      >
        <div class="space-y-0.5">
          <span class="text-base">Receive clipboard text</span>
        </div>
        <Switch :checked="recv_cb" @update:checked="toggle_cb_recv()" />
      </div>
    </div>

    <div class="devices">
      <ConnectedDevices
        v-for="device in devices"
        :key="device.deviceID"
        :deviceID="device.device_id"
        :deviceName="device.device_name"
        :ipAddress="device.ip_address"
        :isOnline="device.is_online"
      />
    </div>
  </div>
</template>
<style scoped>
.main-actions {
  margin-bottom: 32px;
  font-weight: 300;
  gap: 10px;
  display: flex;
  flex-direction: column;
  font-size: 14px;
  .item {
    gap: 2px;
    display: flex;
    align-items: center;
  }
}
.dev-list {
  margin-top: 10px;
  border: 1px solid gray;
  border-radius: 10px;
  height: 250px;
}
.settings-icon {
  position: absolute;
  right: 0;
  top: 0;
  button {
    width: 24px;
    background: transparent;
    border: none;
    padding: 0;
    cursor: pointer;
  }
}
.home-container {
  position: relative;
  padding: 10px;
}
.settings-popup {
  z-index: 10;
  padding: 8px 12px;
  position: absolute;
  border: 1px solid #aeaeae;
  height: 80%;
  width: 80%;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  border-radius: 10px;
  background-color: rgba(50, 50, 50, 0.99);
  .close-popup {
    fill: white;
    width: 22px;
    position: absolute;
    right: 10px;
    top: 10px;
    cursor: pointer;
  }
  .close-popup:hover {
    scale: 1.05;
  }
}
.btnDisabled {
  opacity: 60%;
  cursor: not-allowed;
}
.gkey-gen {
  margin-top: 10px;
  .action {
    display: flex;
    gap: 6px;
    margin-top: 4px;

    input {
      color: #cdcdcd;
      background-color: #121212;
      border: solid 1px #787878;
      padding: 2px 2px;
    }
  }
  button {
    background-color: #121212;
    border: solid 1px #787878;
    color: #cdcdcd;
    padding: 2px 8px;
    text-align: center;
    text-decoration: none;
    font-size: 13px;
    border-radius: 6px;
  }

  display: flex;
  flex-direction: column;
  span {
    font-size: 13px;
    font-weight: 300;
  }
}
.join_req {
  width: 90%;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  color: black;
  background-color: #aeaeae;
  padding: 4px 8px;
  border-radius: 6px;
  display: flex;
  position: absolute;
  gap: 20px;
  justify-items: center;
  align-items: center;
  z-index: 10;

  .actions {
    button {
      background-color: #fff;
      font-weight: 500;
      cursor: pointer;
      margin-right: 6px;
      border: none;
      color: black;
      padding: 2px 6px;
      text-align: center;
      text-decoration: none;
      display: inline-block;
      font-size: 13px;
    }
    button:hover {
      scale: 1.03;
    }

    position: absolute;
    right: 10px;
    bottom: 10px;
    display: flex;
    gap: 10px;
    flex-direction: column;
  }

  .details {
    width: 70%;
    font-size: 14px;
    word-wrap: break-word;
  }

  .label {
    color: #343434;
    font-weight: 600;
    margin-right: 4px;
  }
}
.join_req_clg {
  position: absolute;
  padding: 4px 10px;
  cursor: pointer;
  top: 0;
  right: 0;
}
.join_req_clg:hover {
  scale: 1.3;
}
.devices {
  display: flex;
  gap: 10px;
  flex-wrap: wrap;
}
</style>
