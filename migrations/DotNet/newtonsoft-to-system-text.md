# Newtonsoft.Json to System.Text.Json Migration Guide

You are migrating C# code from Newtonsoft.Json to the native System.Text.Json library.

### Critical API Changes:
1. Serialization: Replace `JsonConvert.SerializeObject(obj)` with `JsonSerializer.Serialize(obj)`.
2. Deserialization: Replace `JsonConvert.DeserializeObject<T>(json)` with `JsonSerializer.Deserialize<T>(json)`.
3. Property Names: Replace the `[JsonProperty("name")]` attribute with `[JsonPropertyName("name")]`.
4. Namespaces: The new serialization methods are in `System.Text.Json` and the attributes are in `System.Text.Json.Serialization`.

### Example BEFORE:
    using Newtonsoft.Json;

    public class User {
        [JsonProperty("user_name")]
        public string Name { get; set; }
        
        [JsonIgnore]
        public string Password { get; set; }
    }

    public class UserService {
        public void Process() {
            string json = JsonConvert.SerializeObject(user);
            User u = JsonConvert.DeserializeObject<User>(json);
        }
    }

### Example AFTER:
    using System.Text.Json;
    using System.Text.Json.Serialization;

    public class User {
        [JsonPropertyName("user_name")]
        public string Name { get; set; }
        
        [JsonIgnore]
        public string Password { get; set; }
    }

    public class UserService {
        public void Process() {
            string json = JsonSerializer.Serialize(user);
            User u = JsonSerializer.Deserialize<User>(json);
        }
    }