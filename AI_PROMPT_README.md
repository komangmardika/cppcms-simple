Author: I Komang Mardika
AI Author: Claude Clode Model Opus 4.8

# the steps from readme.md is not working and show an error since i am not familiar with the framework i'll let claude code to fix everything until it works

i tried to run this but getting error please fix :D

i've been asked to do the task, task is create a table name person, columns are id int 10 required auto increment primary key, name varchar 100 required, email varchar 100 required unique, address varchar 255 not required nullable; the table can be stored to db.db sqlite in this root project without implementing a migration script; i also asked to create class Person which representated person table, the api should provide non-null properties only for example if user address is null you dont need to include in json response; please split logic into a service or helper class; and please add unit test for crud; for reusable function please move to other location like in service or helper and please use DRY and SOLID principal, but don't change anything from User.cpp because it is an example

for put i think it should be fixed, it give me error when try to update data of existing person, can i just update what i sent or in payload only? sometime you just want to add address when it previously null, and when i send everything, it said email already exists since it unique
