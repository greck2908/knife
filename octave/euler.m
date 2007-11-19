function cu = euler(t,cu)

  n = 30;

  if (nargin < 1 )
    t = 0.95;
  end

  if (nargin < 2 )
    gamma = 1.4;
    gm1  = gamma - 1.0;

    rho0 = 1.0;
    c0 = 1.0;
    u0 = 0.3;
    p0 = rho0*c0*c0 / gamma;
    e0 = p0/gm1 + 0.5*rho0*u0*u0;

    cu = [ rho0*ones(n,1) rho0*u0*ones(n,1) e0*ones(n,1) ];
  end

  xi = linspace(-1.5,1.5,n+1)';
  sigma = 0.2;
  h = 1.0 - (1.0-t)*exp(-xi.^2./2.0/sigma/sigma);

  x = 0.5*(xi(1:n)+xi(2:n+1));
  vol = (xi(2:n+1)-xi(1:n)).*0.5.*(h(2:n+1)+h(1:n));

  for iter = 1:100
    for subiter = 1:n
      res = euler_res(x,xi,h,cu);
      cu = cu + 0.8*res/n;
    end

    res_l2 = norm(res)/n;
    up = c2p(cu);
    plot(x,up(:,1),"-;rho;",x,up(:,2),"-;u;",x,up(:,3),"-;p;",xi,h,"-;h;");
    axis([-1.5 1.5 0 2])
  end

end

res_l2 = norm(res)/n;