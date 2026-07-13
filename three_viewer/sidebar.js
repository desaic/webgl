export function setupResizableSidebar(sidebar, resizer, {
  minWidth = 180,
  maxWidthRatio = 0.6,
} = {}) {
  let dragging = false;

  const onDown = (e) => {
    e.preventDefault();
    dragging = true;
    resizer.classList.add("dragging");
    document.body.style.cursor = "col-resize";
    document.body.style.userSelect = "none";
  };

  const onMove = (e) => {
    if (!dragging) return;
    const max = window.innerWidth * maxWidthRatio;
    const w = Math.min(Math.max(e.clientX, minWidth), max);
    sidebar.style.width = w + "px";
  };

  const onUp = () => {
    if (!dragging) return;
    dragging = false;
    resizer.classList.remove("dragging");
    document.body.style.cursor = "";
    document.body.style.userSelect = "";
  };

  resizer.addEventListener("mousedown", onDown);
  window.addEventListener("mousemove", onMove);
  window.addEventListener("mouseup", onUp);
}
