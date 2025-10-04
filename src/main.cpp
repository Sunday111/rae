#include "application.hpp"

int main()
{
    auto& app = Application::Instance();
    app.Init();
    app.Start();
}
