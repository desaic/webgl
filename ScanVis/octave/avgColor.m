function a = avgColor(img)
  %ignore 10 pixels of margin in all directions
  margin = 10;
  
  s = size(img);
  numChannels = 1;
  if(size(s,2)>2)
    numChannels = s(3);
  endif
  a = zeros(numChannels, 1);
  
  if  (s(1)<3*margin || s(2)<3*margin)
    fprintf(1, 'image too small\n');
    return
  end
  cropped = img( (margin+1): (s(1)-margin), (margin+1):(s(2)-margin), :);
  cs = size(cropped);
  numPixels = cs(1)*cs(2);
  for c=1:numChannels
    ss = sum(sum(cropped(:,:,c)));
    a(c) = ss/numPixels;
  endfor
endfunction
