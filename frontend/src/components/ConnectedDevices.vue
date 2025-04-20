<script setup>
import { CircleMinus } from "lucide-vue-next";
import { Button } from "@/components/ui/button";

import { invoke } from "@tauri-apps/api/core";

import {
  Card,
  CardContent,
  CardDescription,
  CardFooter,
  CardHeader,
  CardTitle,
} from "@/components/ui/card";
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
  Tooltip,
  TooltipContent,
  TooltipProvider,
  TooltipTrigger,
} from "@/components/ui/tooltip";

const props = defineProps({
  deviceName: String,
  deviceID: String,
  isOnline: Boolean,
  ipAddress: String,
});

async function kick_device() {
  await invoke("kick_device", {
    deviceId: props.deviceID,
  });
}
</script>
<template>
  <Card :class="[isOnline ? 'border-t-green-500 border-l-1' : 'text-current']">
    <CardHeader>
      <div class="flex items-center justify-between">
        <CardTitle>{{ deviceName }}</CardTitle>
        <AlertDialog>
          <AlertDialogTrigger as-child>
            <Button variant="ghost" class="w-8 h-8">
              <TooltipProvider>
                <Tooltip>
                  <TooltipTrigger as-child>
                    <Button variant="ghost" class="w-8 h-8">
                      <CircleMinus />
                    </Button>
                  </TooltipTrigger>
                  <TooltipContent>
                    <p>Remove this device from the group</p>
                  </TooltipContent>
                </Tooltip>
              </TooltipProvider>
            </Button>
          </AlertDialogTrigger>
          <AlertDialogContent>
            <AlertDialogHeader>
              <AlertDialogTitle>Are you absolutely sure?</AlertDialogTitle>
              <AlertDialogDescription>
                This action will remove the device from the group.
              </AlertDialogDescription>
            </AlertDialogHeader>
            <AlertDialogFooter>
              <AlertDialogCancel>Cancel</AlertDialogCancel>
              <AlertDialogAction @click="kick_device()"
                >Remove device</AlertDialogAction
              >
            </AlertDialogFooter>
          </AlertDialogContent>
        </AlertDialog>
      </div>

      <CardDescription> {{ deviceID }}</CardDescription>
    </CardHeader>
    <CardContent> </CardContent>
  </Card>
</template>
