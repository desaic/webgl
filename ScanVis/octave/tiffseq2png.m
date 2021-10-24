%prefix = 'checker1108/jcam'
function tiffseq2png(prefix, i0 , i1)
  for i = i0:i1
    infile = sprintf('%s%d.tif', prefix, i);
    try
      img = imread(infile);
      p = tiff2pngLossy(img);
      outfile = sprintf('%s%d.png', prefix, i);
      imwrite(p, outfile);
    catch
      msg = lasterror.message;
      fprintf(1,"%s\n",msg);
    end_try_catch
  endfor
endfunction
