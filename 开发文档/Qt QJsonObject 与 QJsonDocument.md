## Qt: QJsonObject 与 QJsonDocument

在 Qt 的 JSON 体系中，`QJsonDocument` 和 `QJsonObject` 是最核心的两个类。为了方便理解，我们可以用一个通俗的类比来总结它们的关系：

*   **`QJsonDocument` 是“信封”或“集装箱”**：负责对外交互（打包和解包）、负责数据在网络或文件中的**字节流形态**转换。
*   **`QJsonObject` 是“信纸内容”或“货物”**：负责内部数据的存储和读取，是纯粹的**键值对（Key-Value）集合**。

下面是它们的详细总结和对比：

---

### 一、 QJsonDocument（文档/转换器）

**1. 核心定位：**
它是 Qt 应用程序与外部世界（网络、磁盘文件）进行 JSON 数据交换的**唯一桥梁**。它封装了一个完整的 JSON 树结构。

**2. 主要职责：**
*   **解析（输入）**：将外部传来的原始字节流（`QByteArray`）解析成 Qt 内部可以识别的 JSON 对象。
*   **序列化（输出）**：将 Qt 内部的 JSON 对象转换为字节流，用于网络发送或保存到文件。
*   **格式校验**：在解析时检查字节数据是否符合 JSON 语法。如果不符合，解析结果为空。

**3. 常用 API：**
```cpp
// --- 解析 (字节流 -> JSON) ---
[static] QJsonDocument fromJson(const QByteArray &json, QJsonParseError *error = nullptr);

// --- 序列化 (JSON -> 字节流) ---
QByteArray toJson(QJsonDocument::JsonFormat format = Indented) const;

// --- 判断内部包裹的是什么类型 ---
bool isObject() const;  // 里面是不是一个对象 {}
bool isArray() const;   // 里面是不是一个数组 []

// --- 提取里面的货物 ---
QJsonObject object() const;
QJsonArray array() const;
```

---

### 二、 QJsonObject（对象/数据仓库）

**1. 核心定位：**
它代表 JSON 数据结构中的 `{ }`（花括号对象）。它是实际承载数据的容器，内部是一个无序的键值对集合（类似于 C++ 的 `std::map` 或 Qt 的 `QMap`/`QHash`）。

**2. 主要职责：**
*   **存储数据**：以“键（字符串）-> 值”的形式存储具体的数据。
*   **读写操作**：提供类似字典的接口，方便程序对具体的字段进行增、删、改、查。

**3. 常用 API：**
```cpp
// --- 插入/修改数据 ---
void insert(const QString &key, const QJsonValue &value);

// --- 查询数据 ---
QJsonValue value(const QString &key) const; // 获取对应的值
bool contains(const QString &key) const;    // 是否包含某个键

// --- 转换为变体类型 (方便在业务层使用) ---
QVariantMap toVariantMap() const; 
```

---

### 三、 它们与 QJsonValue 的关系

不能不提 `QJsonValue`，它是构成 `QJsonObject` 的基石。
在 `QJsonObject` 中，所有的值（Value）都被包装成 `QJsonValue` 类型。`QJsonValue` 是一个万能类型，它可以判断自己里面装的是什么：

```cpp
QJsonValue val = obj.value("age");
if (val.isDouble()) {
    int age = val.toInt();
}
// 它支持判断 isString(), isBool(), isObject(), isArray(), isNull() 等。
```

---

### 四、 完整生命周期对比图示

在一段网络通信中，这两个类的交互流程如下：

**【发送数据时】（业务层 -> 网络层）**
```text
1. 业务层准备数据：
   QJsonObject jsonObj;
   jsonObj.insert("name", "张三");  // QJsonObject 负责组装数据
   jsonObj.insert("age", 20);

2. 封装为文档：
   QJsonDocument doc(jsonObj);      // 将对象装入文档

3. 转换为字节流发送：
   QByteArray bytes = doc.toJson(); // QJsonDocument 负责序列化
   // 发送 bytes...
```

**【接收数据时】（网络层 -> 业务层）**
```text
1. 收到字节流：
   QByteArray bytes = reply->readAll();

2. 解析为文档：
   QJsonDocument doc = QJsonDocument::fromJson(bytes); // QJsonDocument 负责解析

3. 提取出对象：
   if(doc.isObject()){
       QJsonObject jsonObj = doc.object(); // QJsonObject 负责提供数据

4. 业务层读取数据：
       QString name = jsonObj.value("name").toString();
   }
```

### 总结

*   你要和**字节流、网络、文件**打交道时，用 **`QJsonDocument`**。
*   你要和**具体的业务字段、键值对**打交道时，用 **`QJsonObject`**。
*   它们分工明确，`QJsonDocument` 是外壳和搬运工，`QJsonObject`是内核和货物本身。