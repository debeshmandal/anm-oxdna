/*
 * RodCells.h
 *
 *  Created on: 7/oct/2018
 *      Author: flavio
 */

#ifndef RODCELLS_H_
#define RODCELLS_H_

#include "BaseList.h"
#include <cfloat>
#include <vector>
//#include <set>
//#include <unordered_set>
//#include <list>

/**
 * @brief Implementation of simple simulation cells.
 *
 * Internally, this class uses linked-list to keep track of particles in order to
 * have a computational complexity of O(N).
 */
template<typename number>
class RodCells: public BaseList<number> {
protected:
	int * _heads;
	int * _next;
	int *_cells;
	int _N_cells;
	int _N_cells_side[3];

	size_t _max_size;

	number _sqr_rcut;

	number _rod_cell_rcut;
	number _rod_length;
	int * _n_virtual_sites;
	int _n_part_types;
	int _n_virtual_sites_max;
	int _restrict_to_type;

	//std::set<int> _neighs;
	//std::unordered_set<int> _neighs; // unordered_set are measurably and consistently faster than the other options, but slower than sorting the resulting vector
	//std::list<int> _neighs;
	std::vector<bool> _added;

	void _set_N_cells_side_from_box(int N_cells_side[3], BaseBox<number> *box);
	std::vector<BaseParticle<number> *> _get_neigh_list(BaseParticle<number> *p, bool all);
public:
	RodCells(int &N, BaseBox<number> *box);
	virtual ~RodCells();

	virtual void get_settings(input_file &inp);
	virtual void init(BaseParticle<number> **particles, number rcut);

	virtual bool is_updated();
	virtual void single_update(BaseParticle<number> *p);
	virtual void global_update(bool force_update=false);
	virtual std::vector<BaseParticle<number> *> get_neigh_list(BaseParticle<number> *p);
	virtual std::vector<BaseParticle<number> *> get_complete_neigh_list(BaseParticle<number> *p);

	std::vector<BaseParticle<number> * > whos_there(int idx);
	
	virtual int get_N_cells() { return _N_cells; }
	inline int get_cell_index(const LR_vector<number> &pos);
};

template<>
inline int RodCells<float>::get_cell_index(const LR_vector<float> &pos) {
	int res = (int) ((pos.x / this->_box_sides.x - floorf(pos.x / this->_box_sides.x)) * (1.f - FLT_EPSILON)*_N_cells_side[0]);
	res += _N_cells_side[0]*((int) ((pos.y / this->_box_sides.y - floorf(pos.y / this->_box_sides.y))*(1.f - FLT_EPSILON)*_N_cells_side[1]));
	res += _N_cells_side[0]*_N_cells_side[1]*((int) ((pos.z / this->_box_sides.z - floorf(pos.z / this->_box_sides.z))*(1.f - FLT_EPSILON)*_N_cells_side[2]));
	return res;
}

template<>
inline int RodCells<double>::get_cell_index(const LR_vector<double> &pos) {
	int res = (int) ((pos.x / this->_box_sides.x - floor(pos.x / this->_box_sides.x)) * (1. - DBL_EPSILON)*_N_cells_side[0]);
	res += _N_cells_side[0]*((int) ((pos.y / this->_box_sides.y - floor(pos.y / this->_box_sides.y))*(1. - DBL_EPSILON)*_N_cells_side[1]));
	res += _N_cells_side[0]*_N_cells_side[1]*((int) ((pos.z / this->_box_sides.z - floor(pos.z / this->_box_sides.z))*(1. - DBL_EPSILON)*_N_cells_side[2]));
	return res;
}

#endif /* RODCELLS_H_ */
