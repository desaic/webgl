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

//global variables. fuck it
var moveCmd='';
var promoColor='none';

function onDrop (source, target, piece, newPos, oldPos, orientation) {
  console.log('Source: ' + source)
  console.log('Target: ' + target)
  console.log('Piece: ' + piece)

  //check promo
  const whitePromo = (piece == 'wP' && target.charAt(1) == 8);
  const blackPromo = (piece == 'bP' && target.charAt(1) == 1);
  console.log('promo ' + whitePromo + ' ' + blackPromo);
  moveCmd = "move " + source+' '+target;

  let element = document.getElementById('selectPromo');
  element.value = 'none';
  
  //can't move other pieces in the middle of choosing promotion
  if(promoColor == 'w' || promoColor == 'b'){
	//cancel promotion and 
	//request for fen before promotion move.
	promoColor = 'none';
    ws.send('fen');
	return;
  }

  if(whitePromo){
	promoColor = 'w';
  }else if(blackPromo){
	promoColor = 'b';
  }else{
	promoColor='none';
    ws.send(moveCmd);
  }
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
		promoColor='none';
		console.log("move " + src + " " + dst);
		board.move(src+'-'+dst);
	}else if(cmdName == "clear"){
		promoColor='none';
		board.clear();
	}else if(cmdName == "start"){
		promoColor='none';
		board.start();
	}else if (cmdName == "fen"){
		if(tokens.length < 3){
			console.log("not enough fen args");
			return;
		}
		promoColor='none';
		var fen = tokens[1];
		promoColor='none';
		board.position(fen);
	}
}

$('#Use').on('click', function () {
  board.move('e2-e4')
})

$('#Play').on('click', function () {
	board.move('d2-d4')
})

window.promoCallback = function(e) {
	var choice = e.value;
	if(promoColor == 'w'){
		ws.send(moveCmd+' ' + choice);
	}else if(promoColor == 'b'){
       choice = choice.toLowerCase();
	   ws.send(moveCmd+' ' + choice);
	}
	console.log ('promo ' + e.value);
}