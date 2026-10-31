// Move sounds (short wood taps cut from ignore/wood_table.wav, see ../../tools/extract_sounds.py),
// played with the Web Audio API.
export type SoundName = "move" | "capture" | "castle" | "promotion";

const NAMES: SoundName[] = ["move", "capture", "castle", "promotion"];

let ctx: AudioContext | null = null;
const buffers = new Map<SoundName, AudioBuffer>();

async function loadAll(c: AudioContext) {
  await Promise.all(
    NAMES.map(async (name) => {
      try {
        const res = await fetch(`sounds/${name}.wav`);
        buffers.set(name, await c.decodeAudioData(await res.arrayBuffer()));
      } catch (e) {
        console.warn("cannot load sound " + name, e);
      }
    }),
  );
}

// The context is created and the files are decoded at page load (that needs no gesture), so the
// first move already has its sound. Browsers keep the context suspended until a user gesture, so it
// is resumed on the first one.
export function initSounds() {
  ctx = new AudioContext();
  void loadAll(ctx);
  const resume = () => {
    window.removeEventListener("pointerdown", resume, true);
    window.removeEventListener("keydown", resume, true);
    void ctx?.resume();
  };
  window.addEventListener("pointerdown", resume, true);
  window.addEventListener("keydown", resume, true);
}

export function playSound(name: SoundName) {
  const buf = buffers.get(name);
  if (!ctx || !buf) return;
  if (ctx.state === "suspended") void ctx.resume();
  const src = ctx.createBufferSource();
  src.buffer = buf;
  src.connect(ctx.destination);
  src.start();
}
