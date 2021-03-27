var boardconfig = {
  draggable: true,
  dropOffBoard: 'snapback', // this is the default
  position: 'start',
  onChange: onBoardChange,
  onDrop: onDrop
}
var board = Chessboard('myBoard',boardconfig);

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
	if(tokens.length<2){
		console.log("not enough command args");
		return;
	}
	cmdName = tokens[0];
	if(tokens[0] == "move"){
		if(tokens.length < 3){
			console.log("not enough command args");
			return;
		}
		src = tokens[1];
		dst = tokens[2];
		console.log("move " + src + " " + dst);
		board.move(src+'-'+dst);
	}
}

$('#move1Btn').on('click', function () {
  board.move('e2-e4')
})
