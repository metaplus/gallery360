#include "pch.h"
#pragma warning(disable: 4715)
#include <sqlite_orm/sqlite_orm.h>

namespace sqlite_orm::test
{
    struct User
    {
        int id;
        std::string firstName;
        std::string lastName;
        int birthDate;
        std::shared_ptr<std::string> imageUrl;
        int typeId;
    };

    struct UserType
    {
        int id;
        std::string name;
    };

    TEST(Database, Insert) {
        using std::cout;
        using std::endl;
        auto storage = make_storage(
            "F:/GWorkSet/db.sqlite",
            make_table("users.create",
                       make_column("id", &User::id, autoincrement(), primary_key()),
                       make_column("first_name", &User::firstName),
                       make_column("last_name", &User::lastName, default_value("defaultLastName")),
                       make_column("birth_date", &User::birthDate),
                       make_column("image_url", &User::imageUrl),
                       make_column("type_id", &User::typeId)),
            make_table("user_types",
                       make_column("id", &UserType::id, autoincrement(), primary_key()),
                       make_column("name", &UserType::name, default_value("name_placeholder"))));
        for (auto& [table_name,in_sync] : storage.sync_schema(true)) {
            XLOG(INFO) << table_name << " || " << in_sync << endl;
        }
        User user{ -1, "Jonh", "", 664416000, std::make_shared<std::string>("url_to_heaven"), 3 };
        auto insertedId = storage.insert(user);
        cout << "insertedId = " << insertedId << endl;
        user.id = insertedId;
        User secondUser{ -1, "Alice", "Inwonder", 831168000, {}, 2 };
        insertedId = storage.insert(secondUser);
        secondUser.id = insertedId;
        try {
            auto user = storage.get<User>(insertedId);
            cout << "user = " << user.firstName << " " << user.lastName << endl;
        } catch (std::system_error e) {
            cout << e.what() << endl;
        } catch (...) {
            cout << "unknown exeption" << endl;
        }
    }
}
