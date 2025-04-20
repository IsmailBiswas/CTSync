<script setup>
import { invoke } from "@tauri-apps/api/core";
import { ref, onMounted } from "vue";
import DarkMode from "@/components/DarkMode.vue";
import { LoaderCircle } from "lucide-vue-next";

const loadingMSG = ref("Loading...");
const faildLoading = ref(false);
const getRandomDelay = () =>
    Math.floor(Math.random() * (10 - 5 + 1) + 5) * 1000;
async function page_to_show() {
    await invoke("page_to_show", {});
}

function retry() {
    faildLoading.value = false;
    const randomDelay = getRandomDelay();
    setTimeout(() => {
        faildLoading.value = true;
    }, randomDelay);
}

onMounted(() => {
    page_to_show();
    // Set faildLoading to true after a random delay
    const randomDelay = getRandomDelay();
    setTimeout(() => {
        faildLoading.value = true;
    }, randomDelay);
});
</script>

<template>
    <div class="absolute right-4 top-4">
        <DarkMode />
    </div>

    <div class="flex items-center items-center h-screen gap-3 justify-center">
        <span>Connecting to the server</span>
        <LoaderCircle
            class="animate-spin items-center justify-center"
            :size="24"
        />
    </div>
</template>
