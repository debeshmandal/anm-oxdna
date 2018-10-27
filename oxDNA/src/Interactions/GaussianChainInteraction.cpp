/*
 * GaussianChainInteraction.cpp
 *
 *  Created on: 10/feb/2018
 *      Author: jonah
 */

#include "GaussianChainInteraction.h"
#include <sstream>

template<typename number>
GaussianChainInteraction<number>::GaussianChainInteraction() : BaseInteraction<number, GaussianChainInteraction<number> >() {
	this->_int_map[SPRING_POTENTIAL] = &GaussianChainInteraction<number>::_spring;
	this->_int_map[EXC_VOL] = &GaussianChainInteraction<number>::_exc_vol;

	_is_ka_mixture = false;
	_sigma[0] = _sigma[1] = _sigma[2] = 1.;
	_epsilon[0] = _epsilon[1] = _epsilon[2] = 1.;
	_n[0] = _n[1] = _n[2] = 6;
	_N_A = _N_B = 0;


}

template<typename number>
GaussianChainInteraction<number>::~GaussianChainInteraction() {

}

template<typename number>
void GaussianChainInteraction<number>::get_settings(input_file &inp) {
	IBaseInteraction<number>::get_settings(inp);

	getInputInt(&inp, "LJ_n", _n, 0);
	_n[1] = _n[2] = _n[0];
	// no odd exponents allowed
	if((_n[0] % 2) == 1) throw oxDNAException("LJ_n must be an even integer");

	getInputBool(&inp, "LJ_kob_andersen", &_is_ka_mixture, 0);

	double sigma;
	if(getInputDouble(&inp, "LJ_sigma[2]", &sigma, 0) == KEY_FOUND) {
		_sigma[2] = sigma;
		_sigma[1] = 0.5*(1. + _sigma[2]);
	}

	float rcut = 2.5f;
	getInputFloat(&inp, "LJ_rcut", &rcut, 0);
	this->_rcut = (number) rcut;
}

template<typename number>
void GaussianChainInteraction<number>::init() {
	if(_is_ka_mixture) {
		_sigma[1] = 0.8;
		_sigma[2] = 0.88;
		_epsilon[1] = 1.5;
		_epsilon[2] = 0.5;
	}

	for(int i = 0; i < 3; i++) {
		number rcut = this->_rcut * _sigma[i];
		_sqr_LJ_rcut[i] = SQR(rcut);
		_E_cut[i] = 4. * _epsilon[i] * (pow(_sigma[i]/rcut, (number)2*_n[i]) - pow(_sigma[i]/rcut, (number)_n[i]));
		_sqr_sigma[i] = SQR(_sigma[i]);
	}

	if(_sigma[2] > _sigma[0]) this->_rcut *= _sigma[2];
	this->_sqr_rcut = SQR(this->_rcut);
}

template<typename number>
void GaussianChainInteraction<number>::allocate_particles(BaseParticle<number> **particles, int N) {
	for(int i = 0; i < N; i++) particles[i] = new GCParticle<number>();
}

template<typename number>
void GaussianChainInteraction<number>::read_topology(int N, int *N_strands, BaseParticle<number> **particles) {
	*N_strands = N;
	allocate_particles(particles, N);
	for (int i = 0; i < N; i ++) {
	   particles[i]->index = i;
	   particles[i]->type = 0;
	   particles[i]->strand_id = -1;
	}

	int my_N, my_N_strands;

	char line[2048];
	std::ifstream topology;
	topology.open(this->_topology_filename, ios::in);

	if (!topology.good())
		throw oxDNAException("Can't read topology file '%s'. Aborting",
				this->_topology_filename);

	topology.getline(line, 2040);

	sscanf(line, "%d %d\n", &my_N, &my_N_strands);

	char base;
	int strand, i = 0;
	while (topology.good()) {
		topology.getline(line, 2040);
		if (strlen(line) == 0 || line[0] == '#')
			continue;
		if (i == N)
			throw oxDNAException(
					"Too many particles found in the topology file (should be %d), aborting",
					N);

		int tmpn3, tmpn5;

		//int res = sscanf(line, "%d %s %d %d", &strand, base, &tmpn3, &tmpn5);
		std::stringstream ss(line);
		ss >> strand >> base >> tmpn3 >> tmpn5;
		if(!ss.good())
		{
			throw oxDNAException(
								"Line %d of the topology file has an invalid syntax",
								i + 2);
		}

		int x;
		std::set<int> myneighs;
		ss >> x;
		while(ss.good())
		{
			if(x < 0 || x >= N)
			{
				throw oxDNAException(
									"Line %d of the topology file has an invalid syntax, neigbor has invalid id",
									i + 2);
			}
			myneighs.push_back(x);
			ss >> x;
		}


		GCParticle<number> *p = dynamic_cast< GCParticle<number>  * > (particles[i]);

		if (tmpn3 < 0)
			p->n3 = P_VIRTUAL;
		else
			p->n3 = particles[tmpn3];
		if (tmpn5 < 0)
			p->n5 = P_VIRTUAL;
		else
			p->n5 = particles[tmpn5];

		for(std::set<int>::iterator k = myneighs.begin(); k != myneighs.end(); ++k )
		{
			if(p->index < *k)
			{
			  p->add_bonded_neighbor(dynamic_cast<GCParticle<number>  *> (particles[*k]) );
			}
		}

		// store the strand id
		// for a design inconsistency, in the topology file
		// strand ids start from 1, not from 0
		p->strand_id = strand - 1;


		//TODO CONVERT  BASE TO INTEGER, set it to type and base type

		// the base can be either a char or an integer
		if (strlen(base) == 1) {
			p->type = Utils::decode_base(base[0]);
			p->btype = Utils::decode_base(base[0]);
		} else {
			if (atoi(base) > 0)
				p->type = atoi(base) % 4;
			else
				p->type = 3 - ((3 - atoi(base)) % 4);
			p->btype = atoi(base);
		}

		// until here replacem after here is fine


		if (p->type == P_INVALID)
			throw oxDNAException(
					"Particle #%d in strand #%d contains a non valid base '%c'. Aborting",
					i, strand, base);

		p->index = i;
		i++;

		// here we fill the affected vector
		if (p->n3 != P_VIRTUAL)
			p->affected.push_back(ParticlePair<number>(p->n3, p));
		if (p->n5 != P_VIRTUAL)
			p->affected.push_back(ParticlePair<number>(p, p->n5));
	}

	if (i < N_from_conf)
		throw oxDNAException(
				"Not enough particles found in the topology file (should be %d). Aborting",
				N_from_conf);

	topology.close();

	if (my_N != N_from_conf)
		throw oxDNAException(
				"Number of lines in the configuration file and\nnumber of particles in the topology files don't match. Aborting");

	*N_strands = my_N_strands;























	/*
	*N_strands = N;

	std::ifstream topology(this->_topology_filename, ios::in);
	if(!topology.good()) throw oxDNAException("Can't read topology file '%s'. Aborting", this->_topology_filename);
	char line[512];
	topology.getline(line, 512);
	topology.close();
	sscanf(line, "%*d %d\n", &_N_B);
	_N_A = N - _N_B;

	allocate_particles(particles, N);
	for (int i = 0; i < N; i ++) {
	   particles[i]->index = particles[i]->strand_id = i;
	   particles[i]->type = particles[i]->btype = (i < _N_A) ? P_A : P_B;
	}
	*/
}

template<typename number>
number GaussianChainInteraction<number>::pair_interaction(BaseParticle<number> *p, BaseParticle<number> *q, LR_vector<number> *r, bool update_forces) {
	number energy = pair_interaction_nonbonded(p, q, r, update_forces);
	energy += pair_interaction_bonded(p, q, r, update_forces);

	return energy;
}

template<typename number>
number GaussianChainInteraction<number>::pair_interaction_bonded(BaseParticle<number> *p, BaseParticle<number> *q, LR_vector<number> *r, bool update_forces) {
	LR_vector<number> computed_r(0, 0, 0);
		if(r == NULL) {
			computed_r = this->_box->min_image(p->pos, q->pos);
			r = &computed_r;
	}

	return (number) this->_spring(p,q,r,update_forces);
}

template<typename number>
number GaussianChainInteraction<number>::pair_interaction_nonbonded(BaseParticle<number> *p, BaseParticle<number> *q, LR_vector<number> *r, bool update_forces) {
	LR_vector<number> computed_r(0, 0, 0);
	if(r == NULL) {
		computed_r = this->_box->min_image(p->pos, q->pos);
		r = &computed_r;
	}

	return _exc_volume(p, q, r, update_forces);
}

template<typename number>
void GaussianChainInteraction<number>::check_input_sanity(BaseParticle<number> **particles, int N) {

}

template class GaussianChainInteraction<float>;
template class GaussianChainInteraction<double>;
