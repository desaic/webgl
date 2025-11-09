import {MinesApp} from "./MinesApp"

let app : MinesApp;

declare global {
    // Note the capital "W"
    interface Window { app: MinesApp; }
}

function InitScene() {

  const canvas = document.getElementById("myCanvas");
  app = new MinesApp(canvas as HTMLCanvasElement);

  window.app = app;
}

function Animate(_time) {
  app.render();
  requestAnimationFrame(Animate);
}

InitScene();
Animate(null);
