

#ifndef TEENSY_CORE_CSERIAL_H_
#define TEENSY_CORE_CSERIAL_H_


void cserial_print(const char* text);
void cserial_println(const char* text);
void cserial_print_val(int n);


void clog_print(const char* text);
void clog_println(const char* text);
void clog_print_val(int n);


void cppserial_print(const char* text);
void cppserial_println(const char* text);
void cppserial_print_val(int n);

void cpplog_print(const char* text);
void cpplog_println(const char* text);
void cpplog_print_val(int n);


#endif /* TEENSY_CORE_CSERIAL_H_ */
