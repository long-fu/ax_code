#ifndef YOLOV5_HPP
# define YOLOV5_HPP

# include <iostream>
# include <string>

class Yolov5
{

	public:

		Yolov5();
		Yolov5( Yolov5 const & src );
		~Yolov5();

		Yolov5 &		operator=( Yolov5 const & rhs );

	private:

};

std::ostream &			operator<<( std::ostream & o, Yolov5 const & i );

#endif /* ********************************************************** YOLOV5_H */