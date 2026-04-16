#ifndef YOLOV5_HPP
# define YOLOV5_HPP

# include <iostream>
# include <string>
#include "Engine.hpp"

class Yolov5: public Engine
{

	public:

		explicit Yolov5(std::string modelConfig):Engine(modelConfig){};
		~Yolov5();

		void Postprocess(int picWidth,int picHeight,std::vector<detection::Object> &objects);
		Yolov5( Yolov5 const & src ) = delete;
		Yolov5 &		operator=( Yolov5 const & rhs ) = delete;

	private:

};

std::ostream &			operator<<( std::ostream & o, Yolov5 const & i );

#endif /* ********************************************************** YOLOV5_H */