#include <vector>
#include <unordered_map>
//stolen from https://github.com/patricknraanes/FM

class heap {
		public:
				// Constructor and destructor
				heap();
				~heap();

				// Heap arrays:
				// Keys contains the tentative T-values. The heap is organized 
				// according to these, using heap-sort mechanisms.
				// H2T and T2H contain pointers to and from H and T.
				std::vector<float> Keys;
				std::vector<int64_t> H2T;
				std::unordered_map<int64_t, int64_t> T2H;


				// Interface with external Fast-marching algorithm
				bool isInHeap(int64_t Ind);
				int64_t nElems();
				void insert(float time, int64_t Ind);
				///updates Ind to smaller value. does not work with larger value.
				void update(float time, int64_t Ind);
				float getSmallest(int64_t*Ind);

				// Debugging tools
				int64_t checkHeapProperty(int64_t pInd);
				void print();
				void printTree();

		private:
				
				// Keeps track of the number of elements in heap
				int64_t heapCount;

				// Elementary heap operations
				void upHeap(int64_t Ind);
				void downHeap(int64_t Ind);
				void swapElements(int64_t Ind1, int64_t Ind2);
				
				// Index calculations
				int64_t parentInd(int64_t inInd);
				int64_t leftChildInd(int64_t inInd);
				int64_t rightChildInd(int64_t inInd);
				int64_t lastParentInd();

				// Used by printTree. Does the actual printing, top-down.
				void recurse(int row, int pad, int spacing, int S);
};
