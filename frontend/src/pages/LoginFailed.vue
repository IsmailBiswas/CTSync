<script setup>
import { invoke } from "@tauri-apps/api/core";
import { ref } from "vue";
import { Button } from "@/components/ui/button";
const confirm_redirect_singup = ref(false);
import { Alert, AlertDescription, AlertTitle } from "@/components/ui/alert";
import { Loader2, ShieldX, User } from "lucide-vue-next";

import router from "../router";

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

import DarkMode from "@/components/DarkMode.vue";

const recon_spin = ref(false);
const startLoading = () => {
    recon_spin.value = true;
    setTimeout(() => {
        recon_spin.value = false;
    }, 1000);
};
async function manual_login() {
    startLoading();
    await invoke("try_reconnect", {});
}
async function serverConnectPage() {
    await invoke("delete_local_account");
    router.push({ path: "/server_connect" });
}

</script>
<template>
    <div class="h-screen flex items-center w-full">
        <Alert>
            <ShieldX class="h-5 w-5" />
            <AlertTitle>Login failed</AlertTitle>
            <div class="mt-4">
                <Button @click="manual_login()" variant="outline">
                    Retry Login
                    <Loader2 v-if="recon_spin" class="animate-spin"/>
                </Button>
                <AlertDialog>
                    <AlertDialogTrigger as-child>
                        <Button variant="link">Join Server</Button>
                    </AlertDialogTrigger>
                    <AlertDialogContent>
                        <AlertDialogHeader>
                            <AlertDialogTitle>Are you sure?</AlertDialogTitle>
                            <AlertDialogDescription class="text-red-700"
                                >Your current account information will be
                                deleted if you continue.</AlertDialogDescription
                            >
                        </AlertDialogHeader>
                        <AlertDialogFooter>
                            <AlertDialogCancel>Cancel</AlertDialogCancel>

                            <AlertDialogAction @click="serverConnectPage()"
                                >Continue</AlertDialogAction
                            >
                        </AlertDialogFooter>
                    </AlertDialogContent>
                </AlertDialog>
            </div>
        </Alert>

        <div class="absolute top-4 right-4">
            <DarkMode />
        </div>
    </div>
</template>
