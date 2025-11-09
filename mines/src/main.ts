import { InitImGui, MinesApp } from "./MinesApp"

let app: MinesApp;

declare global {
  // Note the capital "W"
  interface Window { app: MinesApp; }
}

function InitScene() {
  InitImGui().then(() => {
    const canvas = document.getElementById("myCanvas");
    app = new MinesApp(canvas as HTMLCanvasElement);
    window.app = app;
  });

}

function Animate(time) {
  if (app) {
    app.render(time);
  }
  requestAnimationFrame(Animate);

}

InitScene();
Animate(null);
