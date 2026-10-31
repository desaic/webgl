import { MinesApp } from "./MinesApp"
import { initDebugConsole } from "./debugConsole"

let app: MinesApp;

declare global {
  interface Window { app: MinesApp; }
}

let lastFrame = 0;

function Animate(time?: number) {
  // frames that take far longer than 60fps expects usually mean the page was stalled
  if (time !== undefined && lastFrame && time - lastFrame > 100) {
    console.warn("long frame " + Math.round(time - lastFrame) + "ms");
  }
  if (time !== undefined) lastFrame = time;
  if (app) {
    app.render();
  }
  requestAnimationFrame(Animate);
}

initDebugConsole();
app = new MinesApp(document.getElementById("myCanvas") as HTMLCanvasElement);
window.app = app;
Animate();
