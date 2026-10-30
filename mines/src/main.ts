import { MinesApp } from "./MinesApp"

let app: MinesApp;

declare global {
  interface Window { app: MinesApp; }
}

function Animate() {
  if (app) {
    app.render();
  }
  requestAnimationFrame(Animate);
}

app = new MinesApp(document.getElementById("myCanvas") as HTMLCanvasElement);
window.app = app;
Animate();
