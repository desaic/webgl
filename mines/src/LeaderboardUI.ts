import { DIFFICULTIES } from "./Minesweeper";
import {
  NAME_KEY, addEntry, clearEntries, formatMs, getEntries, qualifies,
} from "./Leaderboard";

const LABELS: Record<string, string> = {
  beginner: "Beginner",
  intermediate: "Intermediate",
  expert: "Expert",
};

const label = (cat: string) => LABELS[cat] ?? cat;

// storage key: plain difficulty name for games that used flags, suffixed otherwise
export const boardKey = (cat: string, noFlag: boolean) => (noFlag ? cat + ":noflag" : cat);

// Modal window showing the per-difficulty boards, plus the "new record" name prompt.
export class LeaderboardUI {
  private modal = document.getElementById("lbModal")!;
  private tabs = document.getElementById("lbTabs")!;
  private body = document.getElementById("lbBody") as HTMLTableSectionElement;
  private entryBox = document.getElementById("lbEntry")!;
  private entryMsg = document.getElementById("lbEntryMsg")!;
  private nameInput = document.getElementById("lbName") as HTMLInputElement;
  private cat = "beginner";
  private noFlag = false;
  private highlight = -1;
  private pending: { cat: string; noFlag: boolean; ms: number } | null = null;

  constructor() {
    for (const noFlag of [false, true]) {
      const row = document.createElement("div");
      row.className = "lbTabRow";
      const title = document.createElement("span");
      title.textContent = noFlag ? "No flags" : "With flags";
      row.appendChild(title);
      for (const cat of Object.keys(DIFFICULTIES)) {
        const b = document.createElement("button");
        b.type = "button";
        b.textContent = label(cat);
        b.dataset.cat = cat;
        b.dataset.noflag = noFlag ? "1" : "0";
        b.addEventListener("click", () => {
          this.highlight = -1;
          this.cat = cat;
          this.noFlag = noFlag;
          this.render();
        });
        row.appendChild(b);
      }
      this.tabs.appendChild(row);
    }
    document.getElementById("lbClose")!.addEventListener("click", () => this.close());
    document.getElementById("lbClear")!.addEventListener("click", () => {
      if (confirm("Clear the " + this.title(this.cat, this.noFlag) + " leaderboard?")) {
        clearEntries(boardKey(this.cat, this.noFlag));
        this.highlight = -1;
        this.render();
      }
    });
    document.getElementById("lbSave")!.addEventListener("click", () => this.saveRecord());
    this.nameInput.addEventListener("keydown", (e) => {
      if (e.key === "Enter") this.saveRecord();
    });
    this.modal.addEventListener("mousedown", (e) => {
      if (e.target === this.modal && !this.pending) this.close();
    });
    window.addEventListener("keydown", (e) => {
      if (e.key === "Escape" && this.isOpen() && !this.pending) this.close();
    });
  }

  public isOpen() {
    return !this.modal.hidden;
  }

  private title(cat: string, noFlag: boolean) {
    return label(cat) + (noFlag ? " (no flags)" : "");
  }

  public show(cat: string) {
    this.cat = cat;
    this.noFlag = false;
    this.highlight = -1;
    this.open();
  }

  // call on a win; opens the name prompt if the time makes the board
  public offerRecord(cat: string, noFlag: boolean, ms: number) {
    if (!qualifies(boardKey(cat, noFlag), ms)) return;
    this.pending = { cat, noFlag, ms };
    this.cat = cat;
    this.noFlag = noFlag;
    this.highlight = -1;
    this.entryMsg.textContent = "New " + this.title(cat, noFlag) + " record: " + formatMs(ms) + "! Enter your name:";
    try {
      this.nameInput.value = localStorage.getItem(NAME_KEY) ?? "";
    } catch {
      this.nameInput.value = "";
    }
    this.open();
    this.nameInput.focus();
    this.nameInput.select();
  }

  private saveRecord() {
    if (!this.pending) return;
    const name = this.nameInput.value.trim().slice(0, 16) || "Anonymous";
    try {
      localStorage.setItem(NAME_KEY, name);
    } catch {
      // ignore
    }
    this.highlight = addEntry(boardKey(this.pending.cat, this.pending.noFlag), name, this.pending.ms);
    this.pending = null;
    this.render();
  }

  private open() {
    this.modal.hidden = false;
    this.render();
  }

  private close() {
    this.pending = null;
    this.modal.hidden = true;
  }

  private render() {
    this.entryBox.hidden = !this.pending;
    for (const b of Array.from(this.tabs.querySelectorAll("button")) as HTMLButtonElement[]) {
      b.classList.toggle("active", b.dataset.cat === this.cat && (b.dataset.noflag === "1") === this.noFlag);
      b.disabled = !!this.pending;
    }
    const d = DIFFICULTIES[this.cat];
    document.getElementById("lbSubtitle")!.textContent = d
      ? d.cols + "×" + d.rows + ", " + d.mines + " mines"
      : "";

    this.body.replaceChildren();
    const list = getEntries(boardKey(this.cat, this.noFlag));
    if (list.length === 0) {
      const td = this.body.insertRow().insertCell();
      td.colSpan = 4;
      td.className = "empty";
      td.textContent = "No records yet";
    }
    list.forEach((e, i) => {
      const tr = this.body.insertRow();
      if (i === this.highlight) tr.className = "new";
      tr.insertCell().textContent = String(i + 1);
      tr.insertCell().textContent = e.name;
      tr.insertCell().textContent = formatMs(e.ms);
      tr.insertCell().textContent = new Date(e.date).toLocaleDateString();
    });
  }
}
