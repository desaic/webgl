import { Diamond } from "./Diamond.js";
import { Fluorite } from "./Fluorite.js";
import { Gyroid } from "./Gyroid.js";
import { Octet } from "./Octet.js";
export const DENSITY_STEP = 0.001;

export function SelectArray(typeStr, cellSize) {
  const index = Math.trunc(cellSize).toString();
  let arr = [];
  if (typeStr == "diamond") {
    arr = Diamond[index];
  } else if (typeStr == "gyroid") {
    arr = Gyroid[index];
  } else if (typeStr == "fluorite") {
    arr = Fluorite[index];
  } else if (typeStr == "octet") {
    arr = Octet[index];
  }
  if (arr == undefined) {
    return [];
  }
  return arr;
}
