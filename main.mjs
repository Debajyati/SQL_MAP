import sqlMapFactory from './sql_map.js'; // If using ES modules, adjust path if needed

sqlMapFactory().then((SQLMapModule) => {
  // SQLMapModule is now your compiled C library module instance.

  // Wrap the C functions using cwrap for easier calling from JavaScript.
  const sql_map_create = SQLMapModule.cwrap('sql_map_create', 'number', []); // Returns a pointer (number)
  const sql_map_put = SQLMapModule.cwrap('sql_map_put', null, ['number', 'string', 'number']); // void return, SQLMap*, key, value (pointer as number)
  const sql_map_get = SQLMapModule.cwrap('sql_map_get', 'number', ['number', 'string']); // returns void* (number), SQLMap*, key
  const sql_map_remove = SQLMapModule.cwrap('sql_map_remove', 'number', ['number', 'string']); // returns int, SQLMap*, key
  const sql_map_free = SQLMapModule.cwrap('sql_map_free', null, ['number']); // void return, SQLMap*

  // Now you can use these wrapped JavaScript functions to interact with your C library.

  // 1. Create a SQLMap instance
  const map = sql_map_create();
  console.log("Map created:", map);

  // 2. Put some data into the map.  We'll just store numbers as pointers for simplicity in this example.
  let value1 = 100; // Example value
  let value2 = 200;
  // Allocate memory on the wasm heap and get pointers to store values
  let ptr1 = SQLMapModule._malloc(4); // Assume int is 4 bytes, allocate memory for int
  SQLMapModule.HEAP32[ptr1/4] = value1; // Store value1 at ptr1
  let ptr2 = SQLMapModule._malloc(4);
  SQLMapModule.HEAP32[ptr2/4] = value2;

  sql_map_put(map, "key1", ptr1);
  sql_map_put(map, "key2", ptr2);

  // 3. Get data from the map
  let retrievedPtr1 = sql_map_get(map, "key1");
  let retrievedValue1 = SQLMapModule.HEAP32[retrievedPtr1/4]; // Read integer value from pointer
  console.log("Value for key1:", retrievedValue1); // Should be 100

  let retrievedPtr2 = sql_map_get(map, "key2");
  let retrievedValue2 = SQLMapModule.HEAP32[retrievedPtr2/4];
  console.log("Value for key2:", retrievedValue2); // Should be 200

  // 4. Remove an entry
  sql_map_remove(map, "key1");
  let afterRemoveValue1 = sql_map_get(map, "key1");
  console.log("Value for key1 after remove:", afterRemoveValue1); // Should be 0 (NULL pointer represented as 0)

  // 5. Free the SQLMap and allocated memory
  sql_map_free(map);
  SQLMapModule._free(ptr1); // Important: Free allocated wasm memory
  SQLMapModule._free(ptr2);

  console.log("SQLMap operations completed.");

}).catch((error) => {
  console.error("Error loading SQLMap module:", error);
});
