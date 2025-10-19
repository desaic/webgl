export class FileStream {
  // Constant to signal that the internal buffer is empty and a new chunk needs to be read.
  static NEEDS_BUFFER = Symbol('NEEDS_BUFFER');

  constructor(fileBlob, chunkSize = 1024 * 1024) {
    this.fileBlob = fileBlob;
    this.chunkSize = chunkSize;
    this.offset = 0; // Current read position in the file Blob
    this.lineBuffer = []; // Array of complete lines from the current chunk
    this.lineIndex = 0; // Index of the next line to return from lineBuffer
    this.partialLine = ''; // Holds data for a line spanning two chunks
    this.reader = new FileReader(); // Reusable FileReader instance
  }

  /**
   * Reads the next chunk of the file asynchronously, processes it into lines,
   * and populates the internal lineBuffer.
   * @returns {Promise<boolean>} True if new data was read, false if EOF was already reached.
   */
  async buffer() {
    if (this.offset >= this.fileBlob.size) {
      // Nothing left to read
      return false;
    }

    const start = this.offset;
    const end = Math.min(start + this.chunkSize, this.fileBlob.size);
    const chunk = this.fileBlob.slice(start, end);
    
    // Update offset *before* the async read starts
    this.offset = end; 

    const text = await this._readChunk(chunk);

    // Combine the partial line from the previous chunk with the new text
    const lines = (this.partialLine + text).split('\n');
    
    // The last item in the array is either a complete line (if text ends in \n) 
    // or the partial line for the next chunk.
    this.partialLine = lines.pop(); 

    // Reset buffer index and replace with newly read lines
    this.lineBuffer = lines;
    this.lineIndex = 0;

    return true;
  }

  /**
   * Synchronously attempts to retrieve the next line.
   * @returns {string | Symbol | null} 
   * - string: A complete line of text.
   * - FileStream.NEEDS_BUFFER: Buffer is empty, but more file data exists. Call buffer().
   * - null: Absolute End of File (EOF).
   */
  get_line() {
    // 1. Check if there are buffered lines
    if (this.lineIndex < this.lineBuffer.length) {
      return this.lineBuffer[this.lineIndex++];
    }

    // 2. Buffer is empty. Check for EOF or if we need to buffer.
    
    // Absolute EOF: File offset reached the end AND the partial line buffer is empty.
    if (this.offset >= this.fileBlob.size) {
      // Process the last remaining partial line, if any
      if (this.partialLine) {
        const finalLine = this.partialLine;
        this.partialLine = '';
        return finalLine;
      }
      return null; // Absolute End of File
    }
    
    // Buffer empty, but more file data exists. Signal the user to call buffer().
    return FileStream.NEEDS_BUFFER;
  }

  /**
   * Promisified helper to read a chunk Blob as text.
   * @private
   */
  _readChunk(chunkBlob) {
    return new Promise((resolve, reject) => {
      this.reader.onload = (event) => resolve(event.target.result);
      this.reader.onerror = (event) => reject(event.target.error);
      this.reader.readAsText(chunkBlob);
    });
  }
}