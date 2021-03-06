// 6502 disassembler
// n. landsteiner, mass:werk / electronic tradion 2005; e-tradion.net

// lookup tables

var hextab= ['0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'];
var opctab= [
	['BRK','imp'], ['ORA','inx'], ['???','imp'], ['???','imp'],
	['???','imp'], ['ORA','zpg'], ['ASL','zpg'], ['???','imp'],
	['PHP','imp'], ['ORA','imm'], ['ASL','acc'], ['???','imp'],
	['???','imp'], ['ORA','abs'], ['ASL','abs'], ['???','imp'],
	['BPL','rel'], ['ORA','iny'], ['???','imp'], ['???','imp'],
	['???','imp'], ['ORA','zpx'], ['ASL','zpx'], ['???','imp'],
	['CLC','imp'], ['ORA','aby'], ['???','imp'], ['???','imp'],
	['???','imp'], ['ORA','abx'], ['ASL','abx'], ['???','imp'],
	['JSR','abs'], ['AND','inx'], ['???','imp'], ['???','imp'],
	['BIT','zpg'], ['AND','zpg'], ['ROL','zpg'], ['???','imp'],
	['PLP','imp'], ['AND','imm'], ['ROL','acc'], ['???','imp'],
	['BIT','abs'], ['AND','abs'], ['ROL','abs'], ['???','imp'],
	['BMI','rel'], ['AND','iny'], ['???','imp'], ['???','imp'],
	['???','imp'], ['AND','zpx'], ['ROL','zpx'], ['???','imp'],
	['SEC','imp'], ['AND','aby'], ['???','imp'], ['???','imp'],
	['???','imp'], ['AND','abx'], ['ROL','abx'], ['???','imp'],
	['RTI','imp'], ['EOR','inx'], ['???','imp'], ['???','imp'],
	['???','imp'], ['EOR','zpg'], ['LSR','zpg'], ['???','imp'],
	['PHA','imp'], ['EOR','imm'], ['LSR','acc'], ['???','imp'],
	['JMP','abs'], ['EOR','abs'], ['LSR','abs'], ['???','imp'],
	['BVC','rel'], ['EOR','iny'], ['???','imp'], ['???','imp'],
	['???','imp'], ['EOR','zpx'], ['LSR','zpx'], ['???','imp'],
	['CLI','imp'], ['EOR','aby'], ['???','imp'], ['???','imp'],
	['???','imp'], ['EOR','abx'], ['LSR','abx'], ['???','imp'],
	['RTS','imp'], ['ADC','inx'], ['???','imp'], ['???','imp'],
	['???','imp'], ['ADC','zpg'], ['ROR','zpg'], ['???','imp'],
	['PLA','imp'], ['ADC','imm'], ['ROR','acc'], ['???','imp'],
	['JMP','ind'], ['ADC','abs'], ['ROR','abs'], ['???','imp'],
	['BVS','rel'], ['ADC','iny'], ['???','imp'], ['???','imp'],
	['???','imp'], ['ADC','zpx'], ['ROR','zpx'], ['???','imp'],
	['SEI','imp'], ['ADC','aby'], ['???','imp'], ['???','imp'],
	['???','imp'], ['ADC','abx'], ['ROR','abx'], ['???','imp'],
	['???','imp'], ['STA','inx'], ['???','imp'], ['???','imp'],
	['STY','zpg'], ['STA','zpg'], ['STX','zpg'], ['???','imp'],
	['DEY','imp'], ['???','imp'], ['TXA','imp'], ['???','imp'],
	['STY','abs'], ['STA','abs'], ['STX','abs'], ['???','imp'],
	['BCC','rel'], ['STA','iny'], ['???','imp'], ['???','imp'],
	['STY','zpx'], ['STA','zpx'], ['STX','zpy'], ['???','imp'],
	['TYA','imp'], ['STA','aby'], ['TXS','imp'], ['???','imp'],
	['???','imp'], ['STA','abx'], ['???','imp'], ['???','imp'],
	['LDY','imm'], ['LDA','inx'], ['LDX','imm'], ['???','imp'],
	['LDY','zpg'], ['LDA','zpg'], ['LDX','zpg'], ['???','imp'],
	['TAY','imp'], ['LDA','imm'], ['TAX','imp'], ['???','imp'],
	['LDY','abs'], ['LDA','abs'], ['LDX','abs'], ['???','imp'],
	['BCS','rel'], ['LDA','iny'], ['???','imp'], ['???','imp'],
	['LDY','zpx'], ['LDA','zpx'], ['LDX','zpy'], ['???','imp'],
	['CLV','imp'], ['LDA','aby'], ['TSX','imp'], ['???','imp'],
	['LDY','abx'], ['LDA','abx'], ['LDX','aby'], ['???','imp'],
	['CPY','imm'], ['CMP','inx'], ['???','imp'], ['???','imp'],
	['CPY','zpg'], ['CMP','zpg'], ['DEC','zpg'], ['???','imp'],
	['INY','imp'], ['CMP','imm'], ['DEX','imp'], ['???','imp'],
	['CPY','abs'], ['CMP','abs'], ['DEC','abs'], ['???','imp'],
	['BNE','rel'], ['CMP','iny'], ['???','imp'], ['???','imp'],
	['???','imp'], ['CMP','zpx'], ['DEC','zpx'], ['???','imp'],
	['CLD','imp'], ['CMP','aby'], ['???','imp'], ['???','imp'],
	['???','imp'], ['CMP','abx'], ['DEC','abx'], ['???','imp'],
	['CPX','imm'], ['SBC','inx'], ['???','imp'], ['???','imp'],
	['CPX','zpg'], ['SBC','zpg'], ['INC','zpg'], ['???','imp'],
	['INX','imp'], ['SBC','imm'], ['NOP','imp'], ['???','imp'],
	['CPX','abs'], ['SBC','abs'], ['INC','abs'], ['???','imp'],
	['BEQ','rel'], ['SBC','iny'], ['???','imp'], ['???','imp'],
	['???','imp'], ['SBC','zpx'], ['INC','zpx'], ['???','imp'],
	['SED','imp'], ['SBC','aby'], ['???','imp'], ['???','imp'],
	['???','imp'], ['SBC','abx'], ['INC','abx'], ['???','imp']
];

var addrtab= {
	acc:'A',
	abs:'abs',
	abx:'abs,X',
	aby:'abs,Y',
	imm:'#',
	imp:'impl',
	ind:'ind',
	inx:'X,ind',
	iny:'ind,Y',
	rel:'rel',
	zpg:'zpg',
	zpx:'zpg,X',
	zpy:'zpg,Y'
}

var steptab = {
	imp:1,
	acc:1,
	imm:2,
	abs:3,
	abx:3,
	aby:3,
	zpg:2,
	zpx:2,
	zpy:2,
	ind:3,
	inx:2,
	iny:2,
	rel:2
};


// constructor mods (ie4 fix)

var IE4_keyref;
var IE4_keycoderef;

function IE4_makeKeyref() {
	IE4_keyref= new Array();
	IE4_keycoderef= new Array();
	var hex= new Array('A','B','C','D','E','F');
	for (var i=0; i<=15; i++) {
		var high=(i<10)? i:hex[i-10];
		for (var k=0; k<=15; k++) {
			var low=(k<10)? k:hex[k-10];
			var cc=i*16+k;
			if (cc>=32) {
				var cs=unescape("%"+high+low);
				IE4_keyref[cc]=cs;
				IE4_keycoderef[cs]=cc;
			}
		}
	}
}

function _ie4_strfrchr(cc) {
	return (cc!=null)? IE4_keyref[cc] : '';
}

function _ie4_strchcdat(n) {
	var cs=this.charAt(n);
	return (IE4_keycoderef[cs])? IE4_keycoderef[cs] : 0;
}

if (!String.fromCharCode) {
	IE4_makeKeyref();
	String.fromCharCode=_ie4_strfrchr;
}
if (!String.prototype.charCodeAt) {
	if (!IE4_keycoderef) IE4_makeKeyref();
	String.prototype.charCodeAt=_ie4_strchcdat;
}

// globals

var RAM, pc, startAddr, stopAddr, codeAddr;

// functions

export function disassemble(data) {
	// get addresses
	var ad=loadData(data);
	stopAddr=ad;
	// disassemble
    pc=0;
    startAddr = 0;
    codeAddr = 0;
	pc=startAddr;
    var lines = []
    while (pc<stopAddr){
      var line = disassembleStep();
      lines.push(line);
    }
    return lines;
}

function disassembleStep() {
	var instr, op1, op2, addr, ops, disas, adm, step;
	// get instruction and ops, inc pc
	//pc &= 0xffff;
	instr=ByteAt(pc);
	addr=getHexWord(pc);
	ops=getHexByte(instr);
	disas=opctab[instr][0];
	adm=opctab[instr][1];
	step=steptab[adm];
	if (step>1) {
		op1=getHexByte(ByteAt(pc+1));
		if (step>2) op2=getHexByte(ByteAt(pc+2));
	}
	// format and output to listing
	switch (adm) {
		case 'imm' :
			ops+=' '+op1+'   ';
			disas+=' #$'+op1;
			break;
		case 'zpg' :
			ops+=' '+op1+'   ';
			disas+=' $'+op1;
			break;
		case 'acc' :
			ops+='      ';
			disas+=' A';
			break;
		case 'abs' :
			ops+=' '+op1+' '+op2;
			disas+=' $'+op2+op1;
			break;
		case 'zpx' :
			ops+=' '+op1+'   ';
			disas+=' $'+op1+',X';
			break;
		case 'zpy' :
			ops+=' '+op1+'   ';
			disas+=' $'+op1+',Y';
			break;
		case 'abx' :
			ops+=' '+op1+' '+op2;
			disas+=' $'+op2+op1+',X';
			break;
		case 'aby' :
			ops+=' '+op1+' '+op2;
			disas+=' $'+op2+op1+',Y';
			break;
		case 'iny' :
			ops+=' '+op1+'   ';
			disas+=' ($'+op1+'),Y';
			break;
		case 'inx' :
			ops+=' '+op1+'   ';
			disas+=' ($'+op1+',X)';
			break;
		case 'rel' :
			var opv=ByteAt(pc+1);
			var targ=pc+2;
			if (opv&128) {
				targ-=(opv^255)+1;
			}
			else {
				targ +=opv;
			}
			targ&=0xffff;
			ops+=' '+op1+'   ';
			disas+=' $'+getHexWord(targ);
			break;
		case 'ind' :
			ops+=' '+op1+' '+op2;
			disas+=' ($'+op2+op1+')';
			break;
		default :
			ops+='      ';
	}
	pc=pc+step;
	return addr+'   '+ops+'   '+disas;
}

export function loadData(data) {
	RAM=[];
	for (var i=0; i<data.length; i++) {
		var c=data[i];
		RAM[i]=c;
	}
	return data.length;
}

function ByteAt(addr) {
	return RAM[addr] || 0;
}

function getHexByte(v) {
	return ''+hextab[Math.floor(v/16)]+hextab[v&0x0f];
}

function getHexWord(v) {
	return ''+hextab[Math.floor(v/0x1000)]+hextab[Math.floor((v&0x0f00)/256)]+hextab[Math.floor((v&0xf0)/16)]+hextab[v&0x000f];
}

// eof