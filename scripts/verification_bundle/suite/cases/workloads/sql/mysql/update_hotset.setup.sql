DROP TABLE IF EXISTS vw_update_hotset;
CREATE TABLE vw_update_hotset (
  id INT PRIMARY KEY,
  score INT NOT NULL
);
INSERT INTO vw_update_hotset (id, score) VALUES
  (1, 10),(2, 20),(3, 30),(4, 40),(5, 50),(6, 60),(7, 70),(8, 80);
