#ifndef COULOMB3DLINKACTION_H_
#define COULOMB3DLINKACTION_H_

class Coulomb1DLinkAction;

class Coulomb3DLinkAction {
public:
	Coulomb3DLinkAction(Coulomb1DLinkAction&);
	virtual ~Coulomb3DLinkAction();

	double calculateU0(double reff) const;
	double calculateU1(double reff) const;
	double calculateU2(double reff) const;
	double calculateU3(double reff) const;
	double calculateU4(double reff) const;
private:
	const Coulomb1DLinkAction &coulomb1D;
    const double stau;
};

#endif
