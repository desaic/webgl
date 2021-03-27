var boardconfig = {
  draggable: true,
  dropOffBoard: 'snapback', // this is the default
  position: 'start',
  onChange: onBoardChange,
  onDrop: onDrop
}

var board = Chessboard('myBoard',boardconfig);

var editBoardConfig = {
	draggable: true,
	dropOffBoard: 'trash',
	sparePieces: true
  } 
var editBoard = Chessboard('editBoard',editBoardConfig);

function onBoardChange (oldPos, newPos) {
//  console.log('Position changed:')
//  console.log('Old position: ' + Chessboard.objToFen(oldPos))
//  console.log('New position: ' + Chessboard.objToFen(newPos))
}

function onDrop (source, target, piece, newPos, oldPos, orientation) {
  console.log('Source: ' + source)
  console.log('Target: ' + target)
  console.log('Piece: ' + piece)
  ws.send(source+' '+target);
}

function processBoardCommand(command) {
	tokens = command.split(/\s+/);
	console.log(tokens.length);
	if(tokens.length<1){
		console.log("no command given");
		return;
	}
	cmdName = tokens[0];
	if(cmdName == "move"){
		if(tokens.length < 3){
			console.log("not enough move args");
			return;
		}
		src = tokens[1];
		dst = tokens[2];
		console.log("move " + src + " " + dst);
		board.move(src+'-'+dst);
	}else if(cmdName == "clear"){
		board.clear();
	}else if(cmdName == "start"){
		board.start();
	}else if (cmdName == "fen"){
		if(tokens.length < 3){
			console.log("not enough fen args");
			return;
		}
		var fen = tokens[1];
		board.position(fen);
	}
}

$('#Use').on('click', function () {
  board.move('e2-e4')
})

$('#Play').on('click', function () {
	board.move('d2-d4')
})
  