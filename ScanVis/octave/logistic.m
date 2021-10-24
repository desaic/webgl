function y = logistic(x, p)
  y = p(1) + p(2)./(1+exp(-p(3) * (x-p(4))));
endfunction
