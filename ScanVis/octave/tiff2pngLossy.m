%converts 16 bit color tiff images to 8 bit color png images 
%while increasing the width of image to 2x.
function out = tiff2pngLossy(img)
  %rows cols channels
  s = size(img);
  out = uint8(zeros(s(1), s(2), s(3)));
  out(:,1:s(2), :) = img/256;
endfunction
