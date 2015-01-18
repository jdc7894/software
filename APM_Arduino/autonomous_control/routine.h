#ifndef ROUTINE_H_
#define ROUTINE_H_

#include "flight_control.h"

class Routine {
public:
  Routine(Flight_Control* flight_control) : flight_control_(flight_control),
                          finished_(false) { }
  virtual ~Routine();
  
  bool ExecuteFull() {
    while(!ExecuteCycle());
    set_finished(true);
    return true;
  }

  Flight_Control* flight_control() { return flight_control_; }
  
  void set_finished(bool finished) { finished_ = finished; }
  bool finished() { return finished_; }

protected:
  virtual bool ExecuteCycle() = 0;
  
  Flight_Control* flight_control_;
  bool finished_;
  bool interrupted_;
};

#endif