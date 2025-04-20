<script setup>
import router from "../router";
import { invoke } from "@tauri-apps/api/core";
import { Button } from "@/components/ui/button";
import DarkMode from "@/components/DarkMode.vue";

import { listen } from "@tauri-apps/api/event";
import { ref, watch } from "vue";
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
    DialogContent,
    DialogDescription,
    DialogFooter,
    DialogHeader,
    DialogTitle,
    DialogTrigger,
} from "@/components/ui/dialog";

import { Input } from "@/components/ui/input";
import { TriangleAlert, LoaderCircle, CircleSlash2 } from "lucide-vue-next";
import { ChevronLeft } from "lucide-vue-next";

const invite_key = ref("");
const waiting_approval = ref(false);
const no_member_online = ref(false);
const invalid_invite_key = ref(false);

const empty_key_input = ref(false);
watch(invite_key, (newValue) => {
    empty_key_input.value = newValue.length === 0;
});
function close_join_status() {
    waiting_approval.value = false;
    no_member_online.value = false;
    invalid_invite_key.value = false;
}


async function join_new_server(){
    await invoke("delete_local_account");
    router.push({ path: "/server_connect" });
}


async function create_group() {
    console.log("calling create_new_group function...");
    await invoke("create_new_group", {});
}
async function join_group() {
    if (invite_key.value) {
        await invoke("join_group", {
            inviteKey: invite_key.value,
        });
    } else {
        empty_key_input.value = true;
    }
}

listen("join_group_wait", (event) => {
    waiting_approval.value = true;
});


function hide_error_msg() {
    empty_key_input.value = false;
}
</script>

<template>
    <div class="absolute right-4 top-4">
        <DarkMode />
    </div>
    <p class="text-3xl py-4">Join a Group</p>
    <div class="group-conf flex flex-col gap-4">
        <div>
            <Input
                class="max-w-96"
                v-model="invite_key"
                type="text"
                placeholder="Enter group invite key"
            />
            <span
                class="text-sm dark:text-orange-400 text-red-600 dark:font-light"
                v-if="empty_key_input"
                >Please provide an invite key</span
            >
        </div>

        <Dialog v-model:open="no_member_online">
            <DialogContent
                class="sm:max-w-[425px] grid-rows-[auto_minmax(0,1fr)_auto] p-0 max-h-[90dvh]"
            >
                <DialogHeader class="p-6 pb-0">
                    <DialogTitle>
                        <div class="flex items-center gap-2">
                            <TriangleAlert />
                            No device online
                        </div>
                    </DialogTitle>
                    <DialogDescription>
                        <div class="font-light">
                            No device in the requested group is online to
                            approve your request. Please try again later.
                        </div>
                    </DialogDescription>
                </DialogHeader>
            </DialogContent>
        </Dialog>

        <Dialog v-model:open="invalid_invite_key">
            <DialogContent
                class="sm:max-w-[425px] grid-rows-[auto_minmax(0,1fr)_auto] p-0 max-h-[90dvh]"
            >
                <DialogHeader class="p-6 pb-0">
                    <DialogTitle>
                        <div class="flex items-center gap-2">
                            <CircleSlash2 />
                            Invalid invite key
                        </div>
                    </DialogTitle>
                    <DialogDescription>
                        <div class="font-light">
                            The invite key you provided is not valid.
                        </div>
                    </DialogDescription>
                </DialogHeader>
            </DialogContent>
        </Dialog>

        <Dialog v-model:open="waiting_approval">
            <DialogContent
                class="sm:max-w-[425px] grid-rows-[auto_minmax(0,1fr)_auto] p-0 max-h-[90dvh]"
            >
                <DialogHeader class="p-6 pb-0">
                    <DialogTitle>
                        <div class="flex items-center gap-2">
                            <LoaderCircle class="animate-spin" />
                            Waiting for approval
                        </div>
                    </DialogTitle>
                    <DialogDescription>
                        <div class="font-light">
                            Please check other devices in the requested group to
                            approve the join request.
                        </div>
                    </DialogDescription>
                </DialogHeader>
            </DialogContent>
        </Dialog>

        <div class="actions flex gap-2 flex-wrap">
            <Button class="font-light" variant="outline" @click="join_group()" type="button"
                >Join</Button
            >
            <AlertDialog>
                <AlertDialogTrigger as-child>
                    <Button class="font-light opacity-75" variant="outline" type="button">
                        Create New Group
                    </Button>
                </AlertDialogTrigger>
                <AlertDialogContent>
                    <AlertDialogHeader>
                        <AlertDialogTitle>Are you sure?</AlertDialogTitle>
                        <AlertDialogDescription>
                            This will create a new group and you will be added
                            to it.
                        </AlertDialogDescription>
                    </AlertDialogHeader>
                    <AlertDialogFooter>
                        <AlertDialogCancel @click="hide_error_msg()"
                            >Cancel</AlertDialogCancel
                        >
                        <AlertDialogAction @click="create_group()">
                            Create group
                        </AlertDialogAction>
                    </AlertDialogFooter>
                </AlertDialogContent>
            </AlertDialog>
        <Button class="font-light opacity-55" variant="outline" @click="join_new_server()" type="button"
            >Join New Server</Button
        >
        </div>
    </div>
</template>
