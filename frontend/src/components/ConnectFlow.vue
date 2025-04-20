<script setup>
import { invoke } from "@tauri-apps/api/core";
import { listen } from "@tauri-apps/api/event";
import router from "../router";
import { Input } from "@/components/ui/input";
import { Button } from "@/components/ui/button";
import { Checkbox } from "@/components/ui/checkbox";
import { Alert, AlertDescription, AlertTitle } from "@/components/ui/alert";
import DarkMode from "./DarkMode.vue";

import { Label } from "@/components/ui/label";
import {
    NumberField,
    NumberFieldContent,
    NumberFieldDecrement,
    NumberFieldIncrement,
    NumberFieldInput,
} from "@/components/ui/number-field";
import {
    Tooltip,
    TooltipContent,
    TooltipProvider,
    TooltipTrigger,
} from "@/components/ui/tooltip";

import {
    Key,
    KeyRound,
    Loader2,
    LogIn,
    ChevronRight,
    ChevronLeft,
} from "lucide-vue-next";

import { ref, watch, inject } from "vue";
const serverAddress = inject("serverAddress");
const serverPort = inject("serverPort");
const deviceName = inject("deviceName");

const serverKey = ref("secret value");
const serverCertHash = ref("");
const acceptHashView = ref(false);
const hashAccepted = ref(false);
const serverAddressView = ref(false);
const inviteKeyViewToAcceptHashView = ref(false);
const inviteKeyView = ref(false);
const inviteKey = ref("");
const srv_addr_waiting = ref(false);

let randomString = () => Math.random().toString(36).substring(2, 22);

const waiting = ref(false);
const hightlightHashCheck = ref(false);

function toServerAddressView() {
    hightlightHashCheck.value = false;
    acceptHashView.value = false;
    serverAddressView.value = true;
    inviteKeyViewToAcceptHashView.value = false;
    hashAccepted.value = false;
}

listen("server-pub-key-hash", (event) => {
    serverCertHash.value = event.payload;
});

listen("registration-success", (event) => {
    router.push({ path: "group_join" });
});

function tohashAcceptView() {
    inviteKeyView.value = false;
    inviteKeyViewToAcceptHashView.value = true;
    emptyInviteKey.value = false;
    emptyDeviceName.value = false;
}

async function get_sever_public_key_hash() {
    let val = await invoke("get_server_public_key_hash", {
        port: serverPort.value,
        server: serverAddress.value,
    });
}
function hash_accepted() {
    if (hashAccepted.value) {
        inviteKeyView.value = true;
        hightlightHashCheck.value = false;
    } else {
        hightlightHashCheck.value = true;
    }
}
const emptyServerAddress = ref(false);
const invalidServerPort = ref(false);

watch(serverAddress, (newValue) => {
    emptyServerAddress.value = newValue.length === 0;
});
watch(serverPort, (newValue) => {
    invalidServerPort.value = !(
        Number(newValue) >= 1 && Number(newValue) <= 65535
    );
});

async function submit() {
    if (serverAddress.value.length < 1) {
        emptyServerAddress.value = true;
        return;
    }
    if (!(Number(serverPort.value) >= 1 && Number(serverPort.value) <= 65535)) {
        invalidServerPort.value = true;
        return;
    }
    serverCertHash.value = "";

    try {
        srv_addr_waiting.value = true;
        const result = await invoke("get_server_public_key_hash", {
            port: serverPort.value,
            server: serverAddress.value,
        });
    } catch (error) {
        srv_addr_waiting.value = false;
        console.error("Failed to get server public key hash:", error);
        return;
    }

    srv_addr_waiting.value = false;
    acceptHashView.value = true;
    serverAddressView.value = false;
}
const emptyInviteKey = ref(false);
const emptyDeviceName = ref(false);
watch(inviteKey, (newValue) => {
    emptyInviteKey.value = newValue.length === 0;
});
watch(deviceName, (newValue) => {
    emptyDeviceName.value = newValue.length === 0;
});

async function join_server() {
    if (inviteKey.value.length < 1) {
        emptyInviteKey.value = true;
        return;
    }

    if (deviceName.value.length < 1) {
        emptyDeviceName.value = true;
        return;
    }

    waiting.value = true;
    await invoke("verify_server_access", {
        inviteKey: inviteKey.value,
    });
}
</script>
<template>
    <div class="absolute top-4 right-4">
        <DarkMode />
    </div>

    <div class="container">
        <div class="content">
            <div
                :class="{
                    animateOut: acceptHashView,
                    reverseAnimateIn: serverAddressView,
                }"
                class="flex flex-col gap-3"
            >
                <div>
                    <Input
                        class="max-w-96 font-light shadow"
                        v-model="serverAddress"
                        type="text"
                        placeholder="Server Address "
                    />
                    <span
                        v-if="emptyServerAddress"
                        class="text-sm dark:font-light dark:text-orange-400 text-red-600"
                        >Server address is required</span
                    >
                </div>

                <div>
                    <NumberField
                        v-model="serverPort"
                        id="age"
                        :min="1"
                        class="max-w-16 font-light"
                    >
                        <NumberFieldContent>
                            <NumberFieldInput placeholder="Port" />
                        </NumberFieldContent>
                    </NumberField>

                    <span
                        v-if="invalidServerPort"
                        class="text-sm dark:font-light dark:text-orange-400 text-red-600"
                        >Provide a positive number less than 65535</span
                    >
                </div>

                <div>
                    <TooltipProvider>
                        <Tooltip>
                            <TooltipTrigger>
                                <Button
                                    class="select-none dark:font-normal font-light"
                                    @click="submit()"
                                    variant="outline"
                                    >Next
                                    <ChevronRight
                                        v-if="!srv_addr_waiting"
                                        class="w-4 h-4"
                                    />
                                    <Loader2
                                        v-if="srv_addr_waiting"
                                        class="w-4 h-4 animate-spin"
                                    />
                                </Button>
                            </TooltipTrigger>
                            <TooltipContent
                                v-if="!serverAddress || !serverPort"
                            >
                                <p class="font-thin">
                                    Provide server address and port number to
                                    continue
                                </p>
                            </TooltipContent>
                        </Tooltip>
                    </TooltipProvider>
                </div>
            </div>

            <div
                :class="{
                    animateIn:
                        acceptHashView &&
                        !inviteKeyView &&
                        !inviteKeyViewToAcceptHashView,
                    reverseAnimateOut: serverAddressView,
                    reverseAnimateIn:
                        inviteKeyViewToAcceptHashView && !inviteKeyView,
                    animateOut: inviteKeyView,
                }"
                class="confirm-hash w-full"
            >
                <div>
                    <Alert class="flex flex-col gap-1 max-w-96">
                        <AlertDescription class="break-words font-light dark:text-green-300 text-green-600">
                            <p>
                                {{ serverCertHash || "" }}
                            </p>
                        </AlertDescription>

                        <AlertTitle
                            class="flex items-center space-x-2"
                            :class="{ 'text-orange-400': hightlightHashCheck }"
                        >
                            <Checkbox
                                :checked="hashAccepted"
                                @update:checked="hashAccepted = $event"
                                type="checkbox"
                                id="acceptHashView"
                                name="acceptHashView"
                            />
                            <label
                                for="acceptHashView"
                                class="text-sm font-light leading-none peer-disabled:cursor-not-allowed peer-disabled:opacity-70"
                            >
                                Accept Hash
                            </label>
                        </AlertTitle>
                    </Alert>
                </div>

                <div class="my-4 flex gap-4 select-none">
                    <Button
                        class="dark:font-normal font-bold"
                        variant="outline"
                        @click="toServerAddressView()"
                    >
                        <ChevronLeft class="w-4 h-4" />
                        Back
                    </Button>
                    <TooltipProvider>
                        <Tooltip>
                            <TooltipTrigger>
                                <Button
                                    class="dark:font-normal font-bold"
                                    variant="outline"
                                    @click="hash_accepted()"
                                >
                                    Next
                                    <ChevronRight class="w-4 h-4" />
                                </Button>
                            </TooltipTrigger>
                            <TooltipContent v-if="!hashAccepted">
                                <p class="font-thin">
                                    Accept server public key hash to continue
                                </p>
                            </TooltipContent>
                        </Tooltip>
                    </TooltipProvider>
                </div>
            </div>

            <div
                :class="{
                    animateIn: inviteKeyView,
                    reverseAnimateOut:
                        inviteKeyViewToAcceptHashView && !inviteKeyView,
                }"
                class="form-3 w-full"
            >
                <div class="flex flex-col gap-2 max-w-96">
                    <div>
                        <Input
                            v-model="inviteKey"
                            type="text"
                            placeholder="Invite key"
                            class="dark:font-normal font-light"
                        />
                        <span
                            v-if="emptyInviteKey"
                            class="text-sm dark:font-light dark:text-orange-400 text-red-600"
                            >Provide an invite key to join the server</span
                        >
                    </div>

                    <div>
                        <Input
                            v-model="deviceName"
                            type="text"
                            placeholder="Device name"
                            class="dark:font-normal font-light"
                        />
                        <span
                            v-if="emptyDeviceName"
                            class="text-sm dark:font-light dark:text-orange-400 text-red-600"
                            >Give this device a name</span
                        >
                    </div>
                </div>

                <Alert v-if="waiting" class="max-w-96 mt-3 shadow">
                    <KeyRound class="h-4 w-4" />
                    <AlertTitle class="font-light"
                        >Generating encryption keys locally
                    </AlertTitle>
                    <AlertDescription class="font-thin">
                        This could take up to a minute. Please wait.
                    </AlertDescription>
                </Alert>
                <div class="ainvite-action flex gap-3 my-3 select-none">
                    <Button
                        variant="outline"
                        @click="tohashAcceptView()"
                        class="dark:font-normal font-bold"
                    >
                        <ChevronLeft class="w-4 h-4" />
                        Back
                    </Button>

                    <TooltipProvider>
                        <Tooltip>
                            <TooltipTrigger>
                                <Button
                                    :disabled="waiting"
                                    class="dark:font-normal font-bold"
                                    variant="outline"
                                    @click="join_server()"
                                    >Join Server
                                    <LogIn v-if="!waiting" class="w-4 h-4" />
                                    <Loader2
                                        v-if="waiting"
                                        class="w-4 h-4 mr-2 animate-spin"
                                    />
                                </Button>
                            </TooltipTrigger>
                            <TooltipContent v-if="!inviteKey || !deviceName">
                                <p class="font-thin">
                                    Provide server invite key and device name to
                                    join the server
                                </p>
                            </TooltipContent>
                        </Tooltip>
                    </TooltipProvider>
                </div>
            </div>
        </div>
    </div>
</template>

<style scoped>
.content {
    position: relative;
    flex: 0 1 auto;
    padding-left: 10px;
}

.hash-text {
    width: 350px;
    font-weight: 300;
    .hash {
        color: orangered;
    }
    p {
        margin: 0;
        word-wrap: break-word; /* Ensures long words break and wrap */
    }
}
.connection-error {
    font-weight: 300;
    padding-bottom: 10px;
    color: red;
}

.back,
.next {
    background-color: #121212;
    cursor: pointer;
    margin-right: 6px;
    border: solid 1px #787878;
    color: #cdcdcd;
    padding: 4px 24px;
    text-align: center;
    text-decoration: none;
    display: inline-block;
    font-size: 16px;
}
.hash-action {
    margin-top: 10px;
}

/* opcacity and pointer-events here and in animateIn are in conflict. This still works because this style is defined before animateIn */
.confirm-hash {
    position: absolute;
    top: 0;
    left: 0;
    opacity: 0;
    pointer-events: none;
}
/* opcacity and pointer-events here and in animateIn are in conflict. This still works because this style is defined before animateIn */

@keyframes spin {
    from {
        transform: rotate(0deg);
    }
    to {
        transform: rotate(360deg);
    }
}

/* Create the spinning class */
.spin {
    animation: spin 0.4s linear infinite; /* Adjust duration and timing as needed */
}

.form-3 {
    position: absolute;
    top: 0;
    left: 0;
    opacity: 0;
    pointer-events: none;
}

@keyframes SlideFadeIn {
    0% {
        opacity: 0;
        transform: translateX(20px);
    }
    100% {
        opacity: 1;
        transform: translateX(0px);
    }
}

@keyframes ReverseSlideFadeIn {
    0% {
        opacity: 0;
        transform: translateX(-30px);
    }
    100% {
        opacity: 1;
        transform: translateX(0px);
    }
}

@keyframes SlideFadeOut {
    0% {
        opacity: 1;
        transform: translateX(0px);
    }
    100% {
        opacity: 0;
        transform: translateX(-100px);
    }
}

@keyframes ReverseSlideFadeOut {
    0% {
        opacity: 1;
        transform: translateX(0px);
    }
    100% {
        opacity: 0;
        transform: translateX(100px);
    }
}

.animateOut {
    pointer-events: none;
    animation: SlideFadeOut 0.1s;
    opacity: 0;
}

.reverseAnimateOut {
    pointer-events: none;
    animation: ReverseSlideFadeOut 0.1s;
}

.animateIn {
    animation: SlideFadeIn 0.2s;
    opacity: 1;
    pointer-events: auto;
}
.reverseAnimateIn {
    animation: ReverseSlideFadeIn 0.2s;
    opacity: 1;
    pointer-events: auto;
}
.accept-hash {
    display: flex;
    align-items: center;
    color: #dfdfdf;
    input {
        width: 18px;
        padding: 18px;
    }
}

.invite-action {
    margin-top: 10px;
    display: flex;
}
.register-loader {
    margin: 0 12px;
    display: flex;
    align-items: center;
    gap: 4px;
    span {
        font-size: 14px;
        color: greenyellow;
        font-weight: 200;
    }
}
.btnDeactive {
    background-color: #232323;
    color: #454545;
    cursor: not-allowed;
}

.get-in-details {
    display: flex;
    flex-direction: column;
    gap: 6px;
    margin-bottom: 12px;
    input {
        width: 100%;
    }
}
</style>
