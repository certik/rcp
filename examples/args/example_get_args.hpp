#include "Teuchos_ConfigDefs.hpp"

inline
void example_get_args(
  int argc
  ,char* argv[]
  ,bool *useA
  ,bool *shareUtility
  )
{
  *useA = false;
  *shareUtility = false;
  for( int k=1; k < argc; ++k ) {
    if      ( 0==strcmp( argv[k], "--useA" ) )         *useA         = true;
    else if ( 0==strcmp( argv[k], "--shareUtility" ) ) *shareUtility = true;
		else {
			std::cerr << "\nError! the argument \'"<<argv[k]<<"\' is not valid!\n";
			exit(1);
    }
  }
}
