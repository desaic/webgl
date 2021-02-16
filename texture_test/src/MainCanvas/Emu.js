import {processorLoop} from "./6502cpu.js"
import {disassemble} from "./disas.js"
export default class Emu {
    constructor(){
        this.regs = new Array(7)
        this.regs.fill(0)
    }
    disas(buf){
        var l = disassemble(buf);
        return l;
    }
}