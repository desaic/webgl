#include <iostream>
#include "TrigMesh.h"
#include "Water.h"
//#include "UILib/UILib.h"

void debugInterpU() {
	
}

int main(int argc, char* argv[]){
  TrigMesh mesh;
	std::cout<<"XD\n";
  Water water;

	while (true) {
		water.Step();
	}
	return 0;
}