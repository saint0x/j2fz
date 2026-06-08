import "./style.css";
import { mountApp } from "./app";

const root = document.querySelector<HTMLDivElement>("#app");

if (!root) {
  throw new Error("missing #app root");
}

void mountApp(root);
