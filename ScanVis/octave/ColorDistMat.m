function dMat = ColorDistMat(img1, img2, center1, center2,x,y)
  dMat = zeros(2,2)
  diff11 = single(img1(y,x))-center1
  dMat(1,1) = norm(diff11);
  diff21 = single(img2(y,x))-center1
  dMat(2,1) = norm(diff21);
  diff12 = single(img1(y,x))-center2
  dMat(1,2) = norm(diff12);
  diff22 = single(img2(y,x))-center2
  dMat(2,2) = norm(diff22);
endfunction
