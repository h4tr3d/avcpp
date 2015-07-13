#include <iostream>

#include "av.h"
#include "dictionary.h"

using namespace std;
using namespace av;

int main()
{
#if 0
    {
        av::Dictionary dict;
    }
#endif

#if 1
    {
        av::Dictionary dict;
        dict.set("test1", "test value");
        dict.set("test2", 31337);

        cout << dict.get("test1") << endl
             << dict.get("test2") << endl;
             //<< dict.get("test3") << endl;

        {
            shared_ptr<char> sptr;
            {
                av::Dictionary::RawStringPtr ptr;
                ptr = dict.toRawStringPtr('=', ';');
                cout << ptr.get() << endl;

                sptr = std::move(ptr);
            }

            cout << sptr.get() << endl;
        }

        cout << dict[1] << endl;
    }


    {
        av::Dictionary dict;
        dict.set("test1", "test value");
        dict.set("test2", 31337);

        cout << endl;
        cout << "ptr: " << (void*)dict.raw() << endl;
        cout << dict.get("test1") << endl
             << dict.get("test2") << endl;

        av::Dictionary dict2 = dict;
        av::Dictionary dict3;
        cout << endl;
        cout << "ptr: " << (void*)dict.raw() << endl;
        cout << dict.get("test1") << endl
             << dict.get("test2") << endl;
        cout << "ptr: " << (void*)dict2.raw() << endl;
        cout << dict2.get("test1") << endl
             << dict2.get("test2") << endl;

        dict3 = dict;
        cout << endl;
        cout << "ptr: " << (void*)dict.raw() << endl;
        cout << dict.get("test1") << endl
             << dict.get("test2") << endl;
        cout << "ptr: " << (void*)dict3.raw() << endl;
        cout << dict3.get("test1") << endl
             << dict3.get("test2") << endl;

        av::Dictionary dict4 = std::move(dict2);
        av::Dictionary dict5;
        cout << endl;
        cout << "ptr: " << (void*)dict2.raw() << endl;
        cout << "ptr: " << (void*)dict4.raw() << endl;
        cout << dict4.get("test1") << endl
             << dict4.get("test2") << endl;

        dict5 = std::move(dict3);
        cout << endl;
        cout << "ptr: " << (void*)dict3.raw() << endl;
        cout << "ptr: " << (void*)dict5.raw() << endl;
        cout << dict5.get("test1") << endl
             << dict5.get("test2") << endl;

    }

    {
        av::Dictionary dict;
        dict.set("test1", "test value");
        dict.set("test2", 31337);
        dict.set("test3", "other value");

        for (auto const &i : dict)
        {
            cout << i.key() << " -> " << i.value() << endl;
        }

        auto it = dict.begin();
        for (int i = 0; it != dict.end(); ++it, ++i)
        {
            cout << (*it).key() << " -> " << (*it).value() << endl;
            cout << it->key() << " -> " << it->value() << endl;
            it->set(to_string(i));
        }

        {
            auto it = dict.cbegin();
            for (int i = 0; it != dict.cend(); ++it, ++i)
            {
                cout << (*it).key() << " -> " << (*it).value() << endl;
                cout << it->key() << " -> " << it->value() << endl;
                //it->set(to_string(i)); // <-- compilation error
            }
        }

        cout << "End\n";
    }

    {
        av::Dictionary dict = {
            {"init1", "value1"},
            {"init2", "value2"}
        };

        for (auto const &i : dict)
        {
            cout << i.key() << " -> " << i.value() << endl;
        }

        dict = {
            {"init3", "value3"},
            {"init4", "value4"},
        };

        for (auto const &i : dict)
        {
            cout << i.key() << " -> " << i.value() << endl;
        }
    }


    {
        av::DictionaryArray arr = {
            {
                {"i1", "v1"},
                {"i2", "v2"}
            },
            {
                {"i3", "v3"},
                {"i4", "v4"}
            }
        };

        auto v = arr[0];
        v.set("i1", "change1");
        auto c = arr[0];
        c.set("i1", "change2");
        auto d = arr[0];

        cout << v.get("i1") << ", " << c.get("i1") << ", " << d.get("i1") << endl;

        auto& e = arr[0];
        e.set("i1", "change3");
        auto& f = arr[0];
        cout << e.get("i1") << ", " << f.get("i1") << endl;

    }

    {
        Dictionary dict;
        dict.parseString("key1=val1;key2=val2", "=", ";");
        dict.parseString("key3=val1;key4=val2", string("="), string(";"));

        error_code ec;
        cout << " str: " << dict.toString(':', ',', ec) << endl;
        cout << " err: " << ec << ", " << ec.message() << endl;
        cout << " str: " << dict.toString(':', '\0', ec) << endl;
        cout << " err: " << ec << ", " << ec.message() << endl;

        auto raw = dict.toRawStringPtr('=', ',', ec);
        cout << " str: " << raw.get() << endl;
        cout << " err: " << ec << ", " << ec.message() << endl;

    }
#endif

#if 1
    {
#if 0
        {
            AVDictionary *rdict = nullptr;
            av_dict_set(&rdict, "rdict", "rvalue", 0);

            av::Dictionary dict = {
                {"dict", "val"}
            };

            dict.assign(rdict);
        }
#endif

#if 0
        {
            Dictionary dict;
            //dict.set("test")
            dict["test"] = "value";
            dict["test2"] = "fdsfdsfdsfdsfsdfsd";

            auto ptr  = dict.release();
            auto ptr2 = dict.rawPtr();

            //
            // simulate external api
            //
            // {
            av_freep(ptr2);
            //av_freep(dict.rawPtr());

            cout << (void*)dict.raw() << endl;

            AVDictionary *rdict = nullptr;
            av_dict_set(&rdict, "rdict", "rvalue", 0);
            *ptr2 = rdict;
            //
            // }

            dict.assign(*ptr2);

        }
#endif

    }
#endif

#if 0
    {
        DictionaryArray arr = {
            {
                {"key1", "val1"}
            },
            {
                {"key2", "val2"}
            },
        };

        auto arr2 = arr;
        auto raw = arr2.raws();

        for (size_t i = 0; i < arr2.size(); ++i)
        {
            auto ptr1 = raw[i];
            auto ptr2 = arr2[i].raw();
            auto ptr3 = arr[i].raw();

            cout << (void*)ptr1 << " vs " << (void*)ptr2 << " vs " << (void*)ptr3 << endl;
        }
    }
#endif

    return 0;
}
