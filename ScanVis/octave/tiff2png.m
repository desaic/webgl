%converts 16 bit color tiff images to 8 bit color png images 
%while increasing the width of image to 2x.
function out = tiff2png(img)
  %rows cols channels
  s = size(img);
  % little endian
  newCols = 2*s(2);
  out = uint8(zeros(s(1), newCols, s(3)));
  out(:,1:s(2), :) = mod(img,256);
  out(:, (s(2)+1):newCols, :) = img/256;
endfunction
