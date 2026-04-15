#include "Yolov5.hpp"

/*
** ------------------------------- CONSTRUCTOR --------------------------------
*/

Yolov5::Yolov5()
{
}

Yolov5::Yolov5( const Yolov5 & src )
{
}


/*
** -------------------------------- DESTRUCTOR --------------------------------
*/

Yolov5::~Yolov5()
{
}


/*
** --------------------------------- OVERLOAD ---------------------------------
*/

Yolov5 &				Yolov5::operator=( Yolov5 const & rhs )
{
	//if ( this != &rhs )
	//{
		//this->_value = rhs.getValue();
	//}
	return *this;
}

std::ostream &			operator<<( std::ostream & o, Yolov5 const & i )
{
	//o << "Value = " << i.getValue();
	return o;
}


/*
** --------------------------------- METHODS ----------------------------------
*/


/*
** --------------------------------- ACCESSOR ---------------------------------
*/


/* ************************************************************************** */