export  function ParseVol(buffer) {
  // Use a DataView to read the header without creating a new buffer
  const dataView = new DataView(buffer);

  // Read the 32-bit unsigned integers for the grid dimensions
  const x = 8 * dataView.getUint32(0, true); // `true` for little-endian
  const y = dataView.getUint32(4, true);
  const z = dataView.getUint32(8, true);

  const totalVoxels = x * y * z;
  const outputVoxels = new Uint8Array(totalVoxels);

  // The voxel data starts at byte 12
  const voxelDataStartByte = 12;

  // The input voxel data is a Uint8Array starting from byte 12
  const inputVoxelBytes = new Uint8Array(buffer, voxelDataStartByte);

  let voxelIndex = 0;

  // Iterate over each byte of the compressed voxel data
  for (let byte of inputVoxelBytes) {
    // For each byte, iterate through its 8 bits
    for (let bitIndex = 0; bitIndex < 8; bitIndex++) {
      // If we've already filled the total number of voxels, stop
      if (voxelIndex >= totalVoxels) {
        break;
      }

      // Use a bitwise AND operation to check if the bit at the current position is 1
      // The mask is `1 << bitIndex` which shifts a 1 to the correct position (e.g., 1, 2, 4, 8, ...)
      const isVoxelSet = (byte & (1 << bitIndex)) !== 0;

      // Store the result as a 0 or 1 in the new Uint8Array
      outputVoxels[voxelIndex] = isVoxelSet ? 1 : 0;
      voxelIndex++;
    }
  }

  return {
    x: x,
    y: y,
    z: z,
    voxels: outputVoxels,
  };
}