#include <stdlib.h>
#include <math.h>
#include <iostream>
#include <iomanip>
#include "heap.hpp"

#define MIN(a,b) ((a)>(b)?(b):(a))
#define ALLOC_INCREMENT 64
heap::heap() {
		heapCount = 0;
}
		
heap::~heap() {
}

int64_t heap::parentInd(int64_t inInd) {
		return (inInd-1)/2;
}

int64_t heap::leftChildInd(int64_t inInd) {
		return inInd*2 + 1;
}

int64_t heap::rightChildInd(int64_t inInd) {
		return inInd*2 + 2;
}

int64_t heap::lastParentInd() {
		return (heapCount-2)/2;
}

void heap::swapElements(int64_t Ind1, int64_t Ind2) {
		double tmpKey;
		int tmpInd;

		if(Ind1==Ind2)
				return;

		// Swap keys
		tmpKey = Keys[Ind1];
		Keys[Ind1] = Keys[Ind2];
		Keys[Ind2] = tmpKey;

		// Swap T2H values
		// NB: Must be done before the H2T swaps
		T2H[H2T[Ind1]] = Ind2;
		T2H[H2T[Ind2]] = Ind1;

		// Swap H2T elems
		tmpInd = H2T[Ind1];
		H2T[Ind1] = H2T[Ind2];
		H2T[Ind2] = tmpInd;
}

void heap::upHeap(int64_t Ind) {
		// Index of parent of Ind
	  int64_t parent;

		while(Ind>0) {
				parent = (Ind-1)/2;
				if (Keys[Ind] < Keys[parent]) {
						swapElements(Ind,parent);
						Ind = parent;
				}
				else {
						break;
				}
		}
}

void heap::downHeap(int64_t Ind) {
		// Index of first child. Add one to get second child.
	int64_t child1, child2, minChild;

		// NB: Special case
		if(heapCount<2)
				return;

		// Loop until the biggest parent node
		while(Ind <= ((heapCount-2)/2)) {

				child1 = 2*Ind+1;
				child2 = child1+1;
				minChild = Ind;

				// Determine the child with the minimal value
				if(Keys[child1]<Keys[Ind]) {
						minChild = child1;
				}
				if( (child2 < heapCount) && (Keys[child2]<Keys[minChild]) ) {
						minChild = child2;
				}

				// If there was a smaller child, swap with Ind
				if(minChild != Ind) {
						swapElements(Ind,minChild);
						Ind = minChild;
				}
				else {
						break;
				}
		}
}


bool heap::isInHeap(int64_t Ind) {
	return T2H.find(Ind)!=T2H.end();
}

int64_t heap::nElems() {
		return heapCount;
}

void heap::insert(float time, int64_t Ind){
		// Insert element at the end of the heap
	if (heapCount >= Keys.size()) {
		Keys.resize(heapCount + ALLOC_INCREMENT);
		H2T.resize(heapCount + ALLOC_INCREMENT);
	}
	Keys[heapCount] = time;
	H2T[heapCount] = Ind;
	T2H[Ind] = heapCount;
		heapCount++; 

		// Ensure the heap is maintained by moving the 
		// new element as necessary upwards in the heap.
		upHeap(heapCount-1);
}
		
void heap::update(float time, int64_t Ind) {
		Keys[T2H[Ind]] = time;

		// Must do one upHead run because maybe this new, lower, time
		// is lower than those of the parents. By design of Fast-Marching
		// it should never be larger though.
		upHeap(T2H[Ind]);
}

float heap::getSmallest(int64_t*Ind) {

		// Smallest element is the first of the heap
	  float heapRoot = Keys[0];

		// Set 'pointer' to the Ind in T of the min heap element
		*Ind = H2T[0];

		// Replace root by the last heap element
		swapElements(0,heapCount-1);

		// Decrement heapCount. 
		// NB: Important to do this before downHeap is run!
		heapCount--;

		// Run downHeap from root. 
		downHeap(0);

		return heapRoot;
}

int64_t heap::checkHeapProperty(int64_t pInd) {
	int64_t lChild = leftChildInd(pInd);
	int64_t rChild = lChild + 1;

		if((lChild < heapCount) && (Keys[lChild] < Keys[pInd])) {
				return pInd;
		}
		if((rChild < heapCount) && (Keys[rChild] < Keys[pInd])) {
				return pInd;
		}

		if((lChild <= lastParentInd()))
				return checkHeapProperty(lChild);
		if((rChild <= lastParentInd()))
				return checkHeapProperty(rChild);
		
		return -1;
}

void heap::print() {
		std::cout<<"\nHeap:  ";
		for (int64_t iter = 0; iter < heapCount; iter++) {
			std::cout<<std::setw(3)<<Keys[iter]<<" ";
		}
}

void heap::recurse(int row, int pad, int spacing, int S) {
		// If not the first row, recursivly call itself
		if (row>1) {
				int newSpacing = ((int) ceil(2.0*((double) spacing) +
									 	((double) S)));
				int newPad = ((int) ceil(((double) pad) + ((double) spacing)/2.0
									 	+ ((double) S)/2.0));
				recurse(row-1, newPad, newSpacing, S);
		}

		// Calculate the first and last elements on the row
		int beg=(pow(2,(row-1))-1);
		int end=MIN((heapCount-1),(pow(2,row)-2));

		// Newline and padding
		std::cout<<("\n");
		for (int i = 0; i < pad; i++) {
			std::cout << (" ");
		}

		// Print elements
		for(int elem=beg; elem<=end; elem++) {
			std::cout << std::setw(S+spacing) << std::setprecision(S) <<Keys[elem]<<" ";
		}
}

void heap::printTree() {
		if (heapCount==0)
				return;

		// Constants
		int S = 3;
		int B = 4;

		int nRows = 1 + floor(log2(heapCount));

		// Call recurse from the last row
	  std::cout<<("\n");
		recurse(nRows,0,B,S);
		std::cout << ("\n\n");
}
