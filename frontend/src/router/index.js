import { createRouter, createWebHistory } from "vue-router";
import ServerConnect from "../pages/ServerConnect.vue";
import GroupJoin from "../pages/GroupJoin.vue";
import SplashScreen from "../pages/SplashScreen.vue";
import HomeView from "../pages/HomeView.vue";
import LoginFailed from "../pages/LoginFailed.vue";

const routes = [
  { path: "/server_connect", component: ServerConnect },
  { path: "/", component: SplashScreen },
  { path: "/home", component: HomeView },
  { path: "/login_failed", component: LoginFailed },
  {
    path: "/group_join",
    component: GroupJoin,
  },
];

const router = createRouter({
  history: createWebHistory(),
  routes,
});

export default router;
