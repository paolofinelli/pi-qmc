#ifndef COULOMB1DLINKACTION_H_
#define COULOMB1DLINKACTION_H_

class Coulomb1DLinkAction {
public:
	Coulomb1DLinkAction(double stau);
	virtual ~Coulomb1DLinkAction();

    double calculateValueAtOrigin() const;
    double calculateU0(double u0, double reff, double taueff) const;
private:
    const double stau;
	const double stauToMinus1;
	const double stauToMinus2;
	const double stauToMinus3;
	const double stauToMinus4;
	const double stauToMinus5;
};

#endif
