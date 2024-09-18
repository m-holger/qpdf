#include <qpdf/assert_test.h>

#include <qpdf/ObjTable.hh>

struct Test
{
    Test() = default;
    Test(int value) :
        value(value)
    {
    }

    operator bool() const noexcept
    {
        return value != 0;
    }

    int value{0};
};

class Table: public ObjTable<Test>
{
  public:
    Table()
    {
        resize(5);
    }

    void
    test()
    {
        for (int i = 0; i < 10; ++i) {
            (*this)[i].value = 2 * i;
            (*this)[1000 + i].value = 2 * (1000 + i);
        }
        for (int i: {50, 60, 70, 98, 99, 100, 101, 150, 198, 199, 200, 201}) {
            (*this)[i].value = 2 * i;
        }
        resize(100);
        for (int i: {1, 99, 100, 105, 110, 120, 205, 206, 207, 210, 215, 217, 218}) {
            (*this)[i].value = 3 * i;
        }
        resize(200);

        for (int i = 1; i < 10; ++i) {
            emplace_back(i);
        }

        for (int i = 1; i < 10; ++i) {
            auto [j, inserted] = try_emplace_back(2 * i + 1);
            assert((j.value < 20) == inserted);
        }

        forEach([](auto i, auto& item) -> void {
            if (i == 1 || i == 200 || i == 1009) {
                item.value += 50;
            }
        });

        assert(find(10)->value == 0);
        assert(!find(224));
        assert(find(1000)->value == 2000);
        find(50)->value += 42;

        forEach([](auto i, auto const& item) -> void {
            if (item.value) {
                std::cout << std::to_string(i) << " : " << std::to_string(item.value) << "\n";
            }
        });

        std::cout << "2000 : " << std::to_string((*this)[2000].value) << "\n";
    }
};

int
main()
{
    Table().test();

    std::cout << "object table tests done\n";
    return 0;
}
