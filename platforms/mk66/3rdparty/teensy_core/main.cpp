extern "C"
{
    void setup(void);
    void loop(void);
    void yield(void);
}

extern "C" int main(void)
{
    setup();
    while (1)
    {
        loop();
        yield();
    }
}
