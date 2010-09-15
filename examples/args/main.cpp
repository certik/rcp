#include "Teuchos_RCP.hpp"
#include "example_get_args.hpp"

// Inject symbols for RCP so we don't need Teuchos:: qualification
using Teuchos::RCP;
using Teuchos::rcp;
using Teuchos::Ptr;
using Teuchos::outArg;

// Abstract interfaces
class UtilityBase {
public:
  virtual ~UtilityBase() {}
  virtual void f() const = 0;
};
class UtilityBaseFactory {
public:
  virtual ~UtilityBaseFactory() {}
  virtual RCP<UtilityBase> createUtility() const = 0;
};

// Concrete implementations
class UtilityA : public UtilityBase {
public:
  void f() const { std::cout<<"\nUtilityA::f() called, this="<<this<<"\n"; }
};
class UtilityB : public UtilityBase {
public:
  void f() const { std::cout<<"\nUtilityB::f() called, this="<<this<<"\n"; }
};
class UtilityAFactory : public UtilityBaseFactory {
public:
  RCP<UtilityBase> createUtility() const { return rcp(new UtilityA()); }
};
class UtilityBFactory : public UtilityBaseFactory {
public:
  RCP<UtilityBase> createUtility() const { return rcp(new UtilityB()); }
};

// Client classes
class ClientA {
public:
  void f( const UtilityBase &utility ) const { utility.f(); }
};
class ClientB {
  RCP<UtilityBase> utility_;
public:
  void initialize(const RCP<UtilityBase> &utility) { utility_=utility; }
  void g(const ClientA &a) { a.f(*utility_); }
};
class ClientC {
  RCP<const UtilityBaseFactory> utilityFactory_;
  RCP<UtilityBase> utility_;
  bool shareUtility_;
public:
  ClientC( const RCP<const UtilityBaseFactory> &utilityFactory, bool shareUtility )
    :utilityFactory_(utilityFactory),
     utility_(utilityFactory->createUtility()),
     shareUtility_(shareUtility) {}
  void h( const Ptr<ClientB> &b ) {
    if (shareUtility_) { b->initialize(utility_); }
    else { b->initialize(utilityFactory_->createUtility()); }
  }
};

// Main program
int main( int argc, char* argv[] )
{
  // Read options from the commandline
  bool useA, shareUtility;
  example_get_args(argc,argv,&useA,&shareUtility);
  // Create factory
  RCP<UtilityBaseFactory> utilityFactory;
  if(useA) utilityFactory = rcp(new UtilityAFactory());
  else     utilityFactory = rcp(new UtilityBFactory());
  // Create clients
  ClientA a;
  ClientB b1, b2;
  ClientC c(utilityFactory, shareUtility);
  // Do some stuff
  c.h(outArg(b1));
  c.h(outArg(b2));
  b1.g(a);
  b2.g(a);
}
