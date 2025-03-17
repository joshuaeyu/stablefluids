#define IX(i,j) ((i)+(N+2)*(j))
#define SWAP(x0,x) {float * tmp=x0;x0=x;x=tmp;}
#define FOR_EACH_CELL for ( i=1 ; i<=N ; i++ ) { for ( j=1 ; j<=N ; j++ ) {
#define END_FOR }}

// Add density or force.
void add_source ( int N, float * x, float * s, float dt )
{
	int i, size=(N+2)*(N+2);
	for ( i=0 ; i<size ; i++ ) x[i] += dt*s[i];
}

// Set (periodic or fixed) boundary conditions.
void set_bnd ( int N, int b, float * x )
{
	int i;

	for ( i=1 ; i<=N ; i++ ) {
		// Torus
		x[IX(0  ,i)] = x[IX(N,i)];	// left boundary
		x[IX(N+1,i)] = x[IX(1,i)];	// right boundary
		x[IX(i,0  )] = x[IX(i,N)];  // top boundary
		x[IX(i,N+1)] = x[IX(i,1)];  // bottom boundary
		// // Solid walls
		// // - For b = 1 and b = 2, which are only used by vel_step, ensures normal component of the velocity
		// //   field is zero at the boundary since no matter should traverse walls
		// x[IX(0  ,i)] = b==1 ? -x[IX(1,i)] : x[IX(1,i)];	// left boundary
		// x[IX(N+1,i)] = b==1 ? -x[IX(N,i)] : x[IX(N,i)];	// right boundary
		// x[IX(i,0  )] = b==2 ? -x[IX(i,1)] : x[IX(i,1)];  // top boundary
		// x[IX(i,N+1)] = b==2 ? -x[IX(i,N)] : x[IX(i,N)];  // bottom boundary
	}
	x[IX(0  ,0  )] = 0.5f*(x[IX(1,0  )]+x[IX(0  ,1)]);	// top-left corner
	x[IX(0  ,N+1)] = 0.5f*(x[IX(1,N+1)]+x[IX(0  ,N)]);	// top-right corner
	x[IX(N+1,0  )] = 0.5f*(x[IX(N,0  )]+x[IX(N+1,1)]);	// bottom-left corner
	x[IX(N+1,N+1)] = 0.5f*(x[IX(N,N+1)]+x[IX(N+1,N)]);	// bottom-right corner
}

// Gauss-Seidel relaxation, an iterative solver for a system of linear equations. Based on LU decomposition.
void lin_solve ( int N, int b, float * x, float * x0, float a, float c )
{
	int i, j, k;

	for ( k=0 ; k<20 ; k++ ) {
		FOR_EACH_CELL
			x[IX(i,j)] = ( x0[IX(i,j)] + a*(x[IX(i-1,j)]+x[IX(i+1,j)]+x[IX(i,j-1)]+x[IX(i,j+1)]) ) / c;
		END_FOR
		set_bnd ( N, b, x );
	}
}

// Diffusion: Exchange between adjacent cells based on viscosity. Gauss-Seidel relaxation method for solving a linear system.
void diffuse ( int N, int b, float * x, float * x0, float diff, float dt )
{
	float a=dt*diff*N*N;
	lin_solve ( N, b, x, x0, a, 1+4*a );
}

// Advection: Follow velocity field. Semi-Lagrangian linear backtrace.
void advect ( int N, int b, float * d, float * d0, float * u, float * v, float dt )
{
	int i, j, i0, j0, i1, j1;
	float x, y, s0, t0, s1, t1, dt0;

	dt0 = dt*N;
	FOR_EACH_CELL
		x = i-dt0*u[IX(i,j)]; y = j-dt0*v[IX(i,j)];
		if (x<0.5f) x=0.5f; if (x>N+0.5f) x=N+0.5f; i0=(int)x; i1=i0+1;
		if (y<0.5f) y=0.5f; if (y>N+0.5f) y=N+0.5f; j0=(int)y; j1=j0+1;
		s1 = x-i0; s0 = 1-s1; t1 = y-j0; t0 = 1-t1;
		d[IX(i,j)] = s0*(t0*d0[IX(i0,j0)]+t1*d0[IX(i0,j1)])+
					 s1*(t0*d0[IX(i1,j0)]+t1*d0[IX(i1,j1)]);
	END_FOR
	set_bnd ( N, b, d );
}

// Mass conservation: Helmholtz-Hodge decomposition (velocity field = incompressible (mass conserving) field + gradient field)
// to provide Poisson equation (a certain linear system), solved using Gauss-Seidel relaxation. Projects a vector field
// onto its divergence-free component
void project ( int N, float * u, float * v, float * p, float * div )
{
	int i, j;

	FOR_EACH_CELL
		div[IX(i,j)] = -0.5f*(u[IX(i+1,j)]-u[IX(i-1,j)]+v[IX(i,j+1)]-v[IX(i,j-1)])/N;
		p[IX(i,j)] = 0;
	END_FOR	
	set_bnd ( N, 0, div ); set_bnd ( N, 0, p );

	lin_solve ( N, 0, p, div, 1, 4 );

	FOR_EACH_CELL
		u[IX(i,j)] -= 0.5f*N*(p[IX(i+1,j)]-p[IX(i-1,j)]);
		v[IX(i,j)] -= 0.5f*N*(p[IX(i,j+1)]-p[IX(i,j-1)]);
	END_FOR
	set_bnd ( N, 1, u ); set_bnd ( N, 2, v );
}

// Density step.
void dens_step ( int N, float * x, float * x0, float * u, float * v, float diff, float dt )
{
	add_source ( N, x, x0, dt );	// source term
	SWAP ( x0, x ); diffuse ( N, 0, x, x0, diff, dt );	// density diffusion
	SWAP ( x0, x ); advect ( N, 0, x, x0, u, v, dt );	// density advection (follows velocity field)
}

// Velocity step.
void vel_step ( int N, float * u, float * v, float * u0, float * v0, float visc, float dt )
{
	add_source ( N, u, u0, dt ); add_source ( N, v, v0, dt );	// force term
	SWAP ( u0, u ); diffuse ( N, 1, u, u0, visc, dt );		// u component diffusion
	SWAP ( v0, v ); diffuse ( N, 2, v, v0, visc, dt );		// v component diffusion
	project ( N, u, v, u0, v0 );	// mass conservation (so that advection behaves better)
	SWAP ( u0, u ); SWAP ( v0, v );
	advect ( N, 1, u, u0, u0, v0, dt ); advect ( N, 2, v, v0, u0, v0, dt );	// velocity advection (follows velocity field)
	project ( N, u, v, u0, v0 );	// mass conservation (for stability)
}

