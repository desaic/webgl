%compute per-pixel L2 norm of a color image.
function n = colorL2(img)
  s = size(img);
  nChannel =s(3);
  m = immultiply(single(img) , single( img));
  n = zeros(s(1), s(2));
  for i = 1:nChannel
    n(:,:) = n(:,:) + m(:,:,i);
  endfor
  n = sqrt(n);
endfunction
