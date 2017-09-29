/* -*- mode: C++; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
// vim:sts=4:sw=4:ts=4:noet:sr:cino=>s,f0,{0,g0,(0,\:0,t0,+0,=s

/* fflas/fflas_pfgemv.inl
 *
 * ========LICENCE========
 * This file is part of the library FFLAS-FFPACK.
 *
 * FFLAS-FFPACK is free software: you can redistribute it and/or modify
 * it under the terms of the  GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 * ========LICENCE========
 *.
 */



namespace FFLAS
{
	
	
	template<class Field, class AlgoT, class FieldTrait>
	typename Field::Element_ptr
	pfgemv(const Field& F,
		   const FFLAS_TRANSPOSE ta,
		   const size_t m,
		   const size_t n,
		   const typename Field::Element alpha,
		   const typename Field::ConstElement_ptr A, const size_t lda,
		   const typename Field::ConstElement_ptr X, const size_t incX,
		   const typename Field::Element beta,
		   typename Field::Element_ptr Y, const size_t incY, 
		   MMHelper<Field, AlgoT, FieldTrait, ParSeqHelper::Parallel<CuttingStrategy::Recursive, StrategyParameter::Threads> > & H){
		
		if (H.parseq.numthreads()>=m){
			fgemv(F, ta,  m, n,  alpha, A, lda, X, incX, beta, Y, incY);
			
		}else{
			typedef MMHelper<Field,AlgoT,FieldTrait,ParSeqHelper::Parallel<CuttingStrategy::Recursive, StrategyParameter::Threads> > MMH_t;
			MMH_t H1(H);
			MMH_t H2(H);
			size_t M2 = m>>1;
			PAR_BLOCK{
				
				if(H1.parseq.numthreads()>1){
					H1.parseq.set_numthreads(H.parseq.numthreads() >> 1);
					H2.parseq.set_numthreads(H.parseq.numthreads() - H1.parseq.numthreads());
				}else{
					H1.parseq.set_numthreads(H.parseq.numthreads());
					H2.parseq.set_numthreads(H.parseq.numthreads());	
				}
				
				typename Field::ConstElement_ptr A1 = A;
				typename Field::ConstElement_ptr A2 = A + M2*lda;
				typename Field::Element_ptr C1 = Y;
				typename Field::Element_ptr C2 = Y + M2;
				
				TASK(CONSTREFERENCE(F,H1) MODE( READ(A1,X) READWRITE(C1)),
					 {pfgemv( F, ta,  M2, n, alpha, A1, lda, X, incX, beta, C1, incY, H1);}
					 );
				
				TASK(MODE(CONSTREFERENCE(F,H2) READ(A2,X) READWRITE(C2)),
					 {pfgemv(F, ta, m-M2, n, alpha, A2, lda, X, incX, beta, C2, incY, H2);}
					 );
			}
			
		}
		return Y;		
		
	}
	
	
	
	template<class Field, class AlgoT, class FieldTrait>
	typename Field::Element_ptr
	pfgemv(const Field& F,
		   const FFLAS_TRANSPOSE ta,
		   const size_t m,
		   const size_t n,
		   const typename Field::Element alpha,
		   const typename Field::ConstElement_ptr A, const size_t lda,
		   const typename Field::ConstElement_ptr X, const size_t incX,
		   const typename Field::Element beta,
		   typename Field::Element_ptr Y, const size_t incY,
		   MMHelper<Field, AlgoT, FieldTrait, ParSeqHelper::Parallel<CuttingStrategy::Row, StrategyParameter::Threads> > & H){
		
		if(H.parseq.numthreads()<2){
			fgemv( F, ta, m, n, alpha, A , lda, X, incX, beta, Y, incY);
		}else{			
			
			PAR_BLOCK{
				using FFLAS::CuttingStrategy::Row;
				using FFLAS::StrategyParameter::Threads;				
				typedef MMHelper<Field,AlgoT,FieldTrait,ParSeqHelper::Parallel<CuttingStrategy::Row, StrategyParameter::Threads> > MMH_t;
				MMH_t HH(H);
				ParSeqHelper::Parallel<CuttingStrategy::Row, StrategyParameter::Threads> pH;
				
				HH.parseq.set_numthreads(H.parseq.numthreads());
				
				FORBLOCK1D(iter, m,	 pH,  
						   TASK(CONSTREFERENCE(F) MODE( READ(A1,X) READWRITE(Y)),
								{
									pfgemv( F, ta, (iter.end()-iter.begin()), n, alpha, A + iter.begin()*lda, lda, X, incX, beta, Y + iter.begin(), incY, HH);
								} 
								)
						   );
			}
		}
		
		return Y;		
		
	}
	
	
//////////////////////////////////////Possible Cache-friendly with blocking///////////////////////////////////////////
	template<class Field>
	void partfgemv(const Field& F,
				   const FFLAS_TRANSPOSE ta,
				   const size_t m,
				   const size_t n,
				   const typename Field::Element alpha,
				   const typename Field::ConstElement_ptr A, const size_t lda,
				   const typename Field::ConstElement_ptr X, const size_t incX,
				   const typename Field::Element beta,
				   typename Field::Element_ptr Y, const size_t incY){
		/*FFLAS::WriteMatrix (std::cout << "A::in ="<< std::endl, F, m, n, A, lda) << std::endl;	
		FFLAS::WriteMatrix (std::cout << "X::in ="<< std::endl, F, n, incX, X, incX) << std::endl;	
		FFLAS::WriteMatrix (std::cout << "Y::in ="<< std::endl, F, m, incY, Y, incY) << std::endl;*/


		typename Field::Element_ptr tmp  = FFLAS::fflas_new(F, m, incY);
		fassign (F, m, incY, Y, incY, tmp, incY);

		fgemv( F, ta, m, n, alpha, A, lda, X, incX, beta, Y, incY); WAIT;

		fadd (F, m, Y, incY, tmp, incY, Y, incY);
		FFLAS::fflas_delete(tmp);
		//FFLAS::WriteMatrix (std::cout << "Y::out ="<< std::endl, F, m, incY, Y, incY) << std::endl;
		return;
	}
	



	
	template<class Field, class AlgoT, class FieldTrait>
	typename Field::Element_ptr
	pfgemv(const Field& F,
		   const FFLAS_TRANSPOSE ta,
		   const size_t m,
		   const size_t n,
		   const typename Field::Element alpha,
		   const typename Field::ConstElement_ptr A, const size_t lda,
		   const typename Field::ConstElement_ptr X, const size_t incX,
		   const typename Field::Element beta,
		   typename Field::Element_ptr Y, const size_t incY,
		   size_t GS_Cache,
		   MMHelper<Field, AlgoT, FieldTrait, ParSeqHelper::Parallel<CuttingStrategy::Row, StrategyParameter::Grain> > & H){

		
		typedef MMHelper<Field,AlgoT,FieldTrait,ParSeqHelper::Parallel<CuttingStrategy::Row, StrategyParameter::Grain> > MMH_t;
		MMH_t HH(H);

		size_t N = min(m,n);
		const int TILE = min(min(m,GS_Cache), min(n,GS_Cache) ); 
		//Compute tiles in each dimension
		const int nEven = N - N%TILE;

		/*FFLAS::WriteMatrix (std::cout << "A:in ="<< std::endl, F, m, n, A, lda) << std::endl;	
		FFLAS::WriteMatrix (std::cout << "X:in ="<< std::endl, F, n, incX, X, incX) << std::endl;	
		FFLAS::WriteMatrix (std::cout << "Y:in ="<< std::endl, F, m, incY, Y, incY) << std::endl;	*/		
				
		SYNCH_GROUP(	
					PAR_BLOCK{ 	//omp_set_num_threads(4);
				using FFLAS::CuttingStrategy::Row;
				using FFLAS::StrategyParameter::Grain;				
				typedef MMHelper<Field,AlgoT,FieldTrait,ParSeqHelper::Parallel<CuttingStrategy::Row, StrategyParameter::Grain> > MMH_t;
				MMH_t HH(H);
				HH.parseq.set_numthreads(H.parseq.numthreads());
				ParSeqHelper::Parallel<CuttingStrategy::Row, StrategyParameter::Grain> pH;
						//Main body of the matrix

						/*for(int ii=0; ii<nEven; ii+=TILE){
							
							fgemv( F, ta, TILE, TILE, alpha, A+ii*lda, lda, X, incX, beta, Y+ii, incY);WAIT;
							
						}*/

				FOR1D(ii, nEven/TILE, pH,  

								{
									fgemv( F, ta, TILE, TILE, alpha, A+TILE*ii*lda, lda, X, incX, beta, Y+ii*TILE, incY);
								} 

						   );

/********************************************************************************
Maybe use FOR2D so that:
(1)taking ii from 0 to N/TILE then replace all ii index by ii*TILE -->DONE
(2)taking jj from 1 to N/TILE then replace all jj index by jj*TILE  ??????
*********************************************************************************/
FOR1D(ii, nEven/TILE, pH, 
						//for(int ii=0; ii<nEven; ii+=TILE)
						{
							
							for(int jj=TILE; jj<nEven; jj+=TILE){
								
								//partfgemv( F, ta, TILE, TILE, alpha, A+lda*ii+jj, lda, X+jj, incX, beta, Y+ii, incY);	WAIT;
partfgemv( F, ta, TILE, TILE, alpha, A+lda*(ii*TILE)+jj, lda, X+jj, incX, beta, Y+(ii*TILE), incY);						
								
							}
						}  
);		
						
						//Right columns in the peel zone around the perimeter of the matrix

						/*for(int jj=0; jj<nEven; jj+=TILE){
							partfgemv( F, ta, TILE, n-nEven, alpha, A+nEven+jj*lda, lda, X+nEven, incX, beta, Y+jj, incY); WAIT;
						}*/



				FOR1D(jj, nEven/TILE, pH,  

								{
									partfgemv( F, ta, TILE, n-nEven, alpha, A+nEven+jj*TILE*lda, lda, X+nEven, incX, beta, Y+jj*TILE, incY);
								} 

						   );

				
						
						//Bottom rows in the peel zone around the perimeter of the matrix
						if( m-nEven>0){
							fgemv( F, ta, m-nEven, TILE, alpha, A+nEven*lda, lda, X, incX, beta, Y+nEven, incY);WAIT;
/*****************************************************************************************
Possible to use FOR1D taking jj from 0 to N/TILE then replace all jj index by jj*TILE ???
******************************************************************************************/
							for(int jj=TILE; jj<nEven; jj+=TILE){
				
								partfgemv( F, ta, m-nEven, TILE, alpha, A+nEven*lda+jj, lda, X+jj, incX, beta, Y+nEven, incY); WAIT;
								
							}
	
				/*FOR1D(jj,  nEven/TILE-1, pH,  

								{

									//std::cout<<"jj="<<jj<<std::endl;WAIT;
                                    //std::cout<<"jj*TILE="<<jj*TILE<<std::endl;WAIT;
//std::cout<<"=====================1==================="<<std::endl;WAIT;
									//if(0!=jj)partfgemv( F, ta, m-nEven, TILE, alpha, A+nEven*lda+(jj)*TILE, lda, X+(jj)*TILE, incX, beta, Y+nEven, incY);WAIT;
partfgemv( F, ta, m-nEven, TILE, alpha, A+nEven*lda+(jj+1)*TILE, lda, X+(jj+1)*TILE, incX, beta, Y+nEven, incY);WAIT;
//std::cout<<"====================2===================="<<std::endl;WAIT;

								} 

						   );*/


							
						}
						
						//Bottom right corner of the matrix
						if( n-nEven>0&&m-nEven>0){
							
							partfgemv( F, ta, m-nEven, n-nEven, alpha, A+nEven*lda+nEven, lda, X+nEven, incX, beta, Y+nEven, incY);WAIT;

						}
						
						
						
					}   //PAR_BLOCK
						); //SYNCH_GROUP
		
		return Y;		
		
	}	
	
} // FFLAS
