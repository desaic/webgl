import { ViewerApp } from "./ViewerApp.js";
import { setupFileHandling } from "./fileLoader.js";
import { setupCurveControls } from "./curvePlotter.js";
import { setupResizableSidebar } from "./sidebar.js";

const canvas = document.getElementById("scene");
const loadBtn = document.getElementById("load-btn");
const fileInput = document.getElementById("obj-input");
const statusEl = document.getElementById("status");

const app = new ViewerApp(canvas);
app.start();

setupFileHandling(app, { loadBtn, fileInput, statusEl });

setupCurveControls(app, {
  formSelect: document.getElementById("curve-form"),
  fermatParams: document.getElementById("fermat-params"),
  weierstrassParams: document.getElementById("weierstrass-params"),
  plotBtn: document.getElementById("plot-btn"),
  statusEl: document.getElementById("curve-status"),
});

setupResizableSidebar(
  document.getElementById("sidebar"),
  document.getElementById("resizer")
);
