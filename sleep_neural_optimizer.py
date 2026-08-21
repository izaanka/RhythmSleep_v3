import os
import sys
import json
import math
import time
import random
from typing import List, Dict, Tuple, Optional, Union, Any

import numpy as np

class NeuralNetworkArchitectureConfig:
    def __init__(
        self,
        input_size: int = 16,
        hidden1_size: int = 32,
        hidden2_size: int = 16,
        output_size: int = 4,
        learning_rate: float = 0.005,
        momentum: float = 0.9,
        weight_decay: float = 0.0001,
        epochs: int = 150,
        batch_size: int = 32,
        optimizer_type: str = "adam",
        loss_type: str = "cross_entropy",
        label_smoothing: float = 0.05,
        focal_gamma: float = 2.0,
        seed: int = 42,
        early_stopping_patience: int = 25,
        lr_scheduler_type: str = "cosine",
        lr_min: float = 0.0001,
        lr_step_size: int = 30,
        lr_gamma: float = 0.5,
        use_augmentation: bool = True,
        augmentation_factor: float = 1.5,
        mixup_alpha: float = 0.2
    ):
        self.input_size = input_size
        self.hidden1_size = hidden1_size
        self.hidden2_size = hidden2_size
        self.output_size = output_size
        self.learning_rate = learning_rate
        self.momentum = momentum
        self.weight_decay = weight_decay
        self.epochs = epochs
        self.batch_size = batch_size
        self.optimizer_type = optimizer_type
        self.loss_type = loss_type
        self.label_smoothing = label_smoothing
        self.focal_gamma = focal_gamma
        self.seed = seed
        self.early_stopping_patience = early_stopping_patience
        self.lr_scheduler_type = lr_scheduler_type
        self.lr_min = lr_min
        self.lr_step_size = lr_step_size
        self.lr_gamma = lr_gamma
        self.use_augmentation = use_augmentation
        self.augmentation_factor = augmentation_factor
        self.mixup_alpha = mixup_alpha
        self.total_weights = (
            (self.input_size * self.hidden1_size + self.hidden1_size) +
            (self.hidden1_size * self.hidden2_size + self.hidden2_size) +
            (self.hidden2_size * self.output_size + self.output_size)
        )

    def to_dict(self) -> Dict[str, Any]:
        return {
            "input_size": self.input_size,
            "hidden1_size": self.hidden1_size,
            "hidden2_size": self.hidden2_size,
            "output_size": self.output_size,
            "learning_rate": self.learning_rate,
            "momentum": self.momentum,
            "weight_decay": self.weight_decay,
            "epochs": self.epochs,
            "batch_size": self.batch_size,
            "optimizer_type": self.optimizer_type,
            "loss_type": self.loss_type,
            "label_smoothing": self.label_smoothing,
            "focal_gamma": self.focal_gamma,
            "seed": self.seed,
            "early_stopping_patience": self.early_stopping_patience,
            "lr_scheduler_type": self.lr_scheduler_type,
            "lr_min": self.lr_min,
            "total_parameters": self.total_weights
        }

class ActivationFunctions:
    @staticmethod
    def relu(z: np.ndarray) -> np.ndarray:
        return np.maximum(0.0, z)

    @staticmethod
    def relu_derivative(z: np.ndarray) -> np.ndarray:
        return np.where(z > 0.0, 1.0, 0.0)

    @staticmethod
    def leaky_relu(z: np.ndarray, alpha: float = 0.01) -> np.ndarray:
        return np.where(z > 0.0, z, z * alpha)

    @staticmethod
    def leaky_relu_derivative(z: np.ndarray, alpha: float = 0.01) -> np.ndarray:
        return np.where(z > 0.0, 1.0, alpha)

    @staticmethod
    def elu(z: np.ndarray, alpha: float = 1.0) -> np.ndarray:
        return np.where(z > 0.0, z, alpha * (np.exp(np.clip(z, -20.0, 20.0)) - 1.0))

    @staticmethod
    def elu_derivative(z: np.ndarray, alpha: float = 1.0) -> np.ndarray:
        return np.where(z > 0.0, 1.0, alpha * np.exp(np.clip(z, -20.0, 20.0)))

    @staticmethod
    def gelu(z: np.ndarray) -> np.ndarray:
        return 0.5 * z * (1.0 + np.tanh(np.sqrt(2.0 / np.pi) * (z + 0.044715 * np.power(z, 3))))

    @staticmethod
    def sigmoid(z: np.ndarray) -> np.ndarray:
        clipped_z = np.clip(z, -30.0, 30.0)
        return 1.0 / (1.0 + np.exp(-clipped_z))

    @staticmethod
    def softmax(z: np.ndarray) -> np.ndarray:
        if z.ndim == 1:
            z_shifted = z - np.max(z)
            exp_z = np.exp(np.clip(z_shifted, -50.0, 50.0))
            sum_exp = np.sum(exp_z)
            if sum_exp < 1e-12:
                return np.full_like(z, 1.0 / len(z))
            return exp_z / sum_exp
        else:
            z_shifted = z - np.max(z, axis=-1, keepdims=True)
            exp_z = np.exp(np.clip(z_shifted, -50.0, 50.0))
            sum_exp = np.sum(exp_z, axis=-1, keepdims=True)
            sum_exp = np.where(sum_exp < 1e-12, 1e-12, sum_exp)
            return exp_z / sum_exp

class LossFunctions:
    @staticmethod
    def cross_entropy(
        y_pred: np.ndarray,
        y_true: np.ndarray,
        label_smoothing: float = 0.0,
        epsilon: float = 1e-12
    ) -> float:
        y_pred_clipped = np.clip(y_pred, epsilon, 1.0 - epsilon)
        num_classes = y_pred.shape[-1]
        if label_smoothing > 0.0:
            smoothed_targets = (1.0 - label_smoothing) * y_true + (label_smoothing / num_classes)
            loss = -np.sum(smoothed_targets * np.log(y_pred_clipped)) / y_pred.shape[0]
        else:
            loss = -np.sum(y_true * np.log(y_pred_clipped)) / y_pred.shape[0]
        return float(loss)

    @staticmethod
    def focal_loss(
        y_pred: np.ndarray,
        y_true: np.ndarray,
        gamma: float = 2.0,
        alpha: Optional[np.ndarray] = None,
        epsilon: float = 1e-12
    ) -> float:
        y_pred_clipped = np.clip(y_pred, epsilon, 1.0 - epsilon)
        pt = np.sum(y_true * y_pred_clipped, axis=-1)
        focal_weight = np.power(1.0 - pt, gamma)
        if alpha is not None:
            alpha_factor = np.sum(y_true * alpha, axis=-1)
            focal_weight = alpha_factor * focal_weight
        loss = -np.mean(focal_weight * np.log(pt))
        return float(loss)

class SGDOptimizer:
    def __init__(self, learning_rate: float = 0.005, momentum: float = 0.9, nesterov: bool = True, weight_decay: float = 1e-4):
        self.learning_rate = learning_rate
        self.momentum = momentum
        self.nesterov = nesterov
        self.weight_decay = weight_decay
        self.v_w = {}
        self.v_b = {}

    def update(self, layer_idx: int, w: np.ndarray, b: np.ndarray, grad_w: np.ndarray, grad_b: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
        if layer_idx not in self.v_w:
            self.v_w[layer_idx] = np.zeros_like(w)
            self.v_b[layer_idx] = np.zeros_like(b)
        
        grad_w_decay = grad_w + self.weight_decay * w
        grad_b_decay = grad_b + self.weight_decay * b

        self.v_w[layer_idx] = self.momentum * self.v_w[layer_idx] - self.learning_rate * grad_w_decay
        self.v_b[layer_idx] = self.momentum * self.v_b[layer_idx] - self.learning_rate * grad_b_decay

        if self.nesterov:
            w_new = w + self.momentum * self.v_w[layer_idx] - self.learning_rate * grad_w_decay
            b_new = b + self.momentum * self.v_b[layer_idx] - self.learning_rate * grad_b_decay
        else:
            w_new = w + self.v_w[layer_idx]
            b_new = b + self.v_b[layer_idx]

        return w_new, b_new

class AdamOptimizer:
    def __init__(
        self,
        learning_rate: float = 0.005,
        beta1: float = 0.9,
        beta2: float = 0.999,
        epsilon: float = 1e-8,
        weight_decay: float = 1e-4
    ):
        self.learning_rate = learning_rate
        self.beta1 = beta1
        self.beta2 = beta2
        self.epsilon = epsilon
        self.weight_decay = weight_decay
        self.m_w = {}
        self.v_w = {}
        self.m_b = {}
        self.v_b = {}
        self.t = 0

    def step(self):
        self.t += 1

    def update(self, layer_idx: int, w: np.ndarray, b: np.ndarray, grad_w: np.ndarray, grad_b: np.ndarray) -> Tuple[np.ndarray, np.ndarray]:
        if layer_idx not in self.m_w:
            self.m_w[layer_idx] = np.zeros_like(w)
            self.v_w[layer_idx] = np.zeros_like(w)
            self.m_b[layer_idx] = np.zeros_like(b)
            self.v_b[layer_idx] = np.zeros_like(b)

        grad_w_decay = grad_w + self.weight_decay * w
        grad_b_decay = grad_b + self.weight_decay * b

        self.m_w[layer_idx] = self.beta1 * self.m_w[layer_idx] + (1.0 - self.beta1) * grad_w_decay
        self.v_w[layer_idx] = self.beta2 * self.v_w[layer_idx] + (1.0 - self.beta2) * np.square(grad_w_decay)
        self.m_b[layer_idx] = self.beta1 * self.m_b[layer_idx] + (1.0 - self.beta1) * grad_b_decay
        self.v_b[layer_idx] = self.beta2 * self.v_b[layer_idx] + (1.0 - self.beta2) * np.square(grad_b_decay)

        t_val = max(1, self.t)
        m_w_hat = self.m_w[layer_idx] / (1.0 - np.power(self.beta1, t_val))
        v_w_hat = self.v_w[layer_idx] / (1.0 - np.power(self.beta2, t_val))
        m_b_hat = self.m_b[layer_idx] / (1.0 - np.power(self.beta1, t_val))
        v_b_hat = self.v_b[layer_idx] / (1.0 - np.power(self.beta2, t_val))

        w_new = w - (self.learning_rate * m_w_hat) / (np.sqrt(v_w_hat) + self.epsilon)
        b_new = b - (self.learning_rate * m_b_hat) / (np.sqrt(v_b_hat) + self.epsilon)

        return w_new, b_new

class CosineAnnealingLRScheduler:
    def __init__(self, optimizer: Any, t_max: int, eta_min: float = 1e-5):
        self.optimizer = optimizer
        self.t_max = t_max
        self.eta_min = eta_min
        self.initial_lr = optimizer.learning_rate
        self.current_step = 0

    def step(self):
        self.current_step += 1
        lr = self.eta_min + 0.5 * (self.initial_lr - self.eta_min) * (
            1.0 + math.cos(math.pi * self.current_step / self.t_max)
        )
        self.optimizer.learning_rate = max(self.eta_min, lr)

class DenseLayer:
    def __init__(self, in_features: int, out_features: int, seed: int = 42):
        self.in_features = in_features
        self.out_features = out_features
        rng = np.random.RandomState(seed)
        limit = np.sqrt(6.0 / (in_features + out_features))
        self.weights = rng.uniform(-limit, limit, size=(in_features, out_features)).astype(np.float32)
        self.biases = np.zeros(out_features, dtype=np.float32)
        self.last_input: Optional[np.ndarray] = None
        self.last_z: Optional[np.ndarray] = None
        self.grad_w: Optional[np.ndarray] = None
        self.grad_b: Optional[np.ndarray] = None

    def forward(self, x: np.ndarray) -> np.ndarray:
        self.last_input = x
        z = np.dot(x, self.weights) + self.biases
        self.last_z = z
        return z

    def backward(self, grad_output: np.ndarray) -> np.ndarray:
        if self.last_input is None:
            raise ValueError("Forward pass must be executed before backward pass.")
        if self.last_input.ndim == 1:
            self.grad_w = np.outer(self.last_input, grad_output)
            self.grad_b = grad_output.copy()
            grad_input = np.dot(self.weights, grad_output)
        else:
            self.grad_w = np.dot(self.last_input.T, grad_output) / self.last_input.shape[0]
            self.grad_b = np.mean(grad_output, axis=0)
            grad_input = np.dot(grad_output, self.weights.T)
        return grad_input

class RhythmSleepMultiLayerPerceptron:
    def __init__(self, config: NeuralNetworkArchitectureConfig):
        self.config = config
        self.layer1 = DenseLayer(config.input_size, config.hidden1_size, seed=config.seed)
        self.layer2 = DenseLayer(config.hidden1_size, config.hidden2_size, seed=config.seed + 1)
        self.layer3 = DenseLayer(config.hidden2_size, config.output_size, seed=config.seed + 2)
        
        if config.optimizer_type.lower() == "adam":
            self.optimizer = AdamOptimizer(
                learning_rate=config.learning_rate,
                weight_decay=config.weight_decay
            )
        else:
            self.optimizer = SGDOptimizer(
                learning_rate=config.learning_rate,
                momentum=config.momentum,
                weight_decay=config.weight_decay
            )

        if config.lr_scheduler_type.lower() == "cosine":
            self.scheduler = CosineAnnealingLRScheduler(
                self.optimizer,
                t_max=config.epochs,
                eta_min=config.lr_min
            )
        else:
            self.scheduler = None

    def forward(self, x: np.ndarray) -> np.ndarray:
        z1 = self.layer1.forward(x)
        a1 = ActivationFunctions.relu(z1)
        z2 = self.layer2.forward(a1)
        a2 = ActivationFunctions.relu(z2)
        z3 = self.layer3.forward(a2)
        a3 = ActivationFunctions.softmax(z3)
        return a3

    def forward_intermediates(self, x: np.ndarray) -> Tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
        z1 = self.layer1.forward(x)
        a1 = ActivationFunctions.relu(z1)
        z2 = self.layer2.forward(a1)
        a2 = ActivationFunctions.relu(z2)
        z3 = self.layer3.forward(a2)
        a3 = ActivationFunctions.softmax(z3)
        return z1, a1, z2, a2, z3, a3

    def backward(self, y_pred: np.ndarray, y_true: np.ndarray):
        grad_out = y_pred - y_true
        grad_a2 = self.layer3.backward(grad_out)
        grad_z2 = grad_a2 * ActivationFunctions.relu_derivative(self.layer2.last_z)
        grad_a1 = self.layer2.backward(grad_z2)
        grad_z1 = grad_a1 * ActivationFunctions.relu_derivative(self.layer1.last_z)
        self.layer1.backward(grad_z1)

    def optimize_step(self):
        if isinstance(self.optimizer, AdamOptimizer):
            self.optimizer.step()

        self.layer1.weights, self.layer1.biases = self.optimizer.update(
            1, self.layer1.weights, self.layer1.biases, self.layer1.grad_w, self.layer1.grad_b
        )
        self.layer2.weights, self.layer2.biases = self.optimizer.update(
            2, self.layer2.weights, self.layer2.biases, self.layer2.grad_w, self.layer2.grad_b
        )
        self.layer3.weights, self.layer3.biases = self.optimizer.update(
            3, self.layer3.weights, self.layer3.biases, self.layer3.grad_w, self.layer3.grad_b
        )

    def scheduler_step(self):
        if self.scheduler is not None:
            self.scheduler.step()

    def get_flattened_weights(self) -> np.ndarray:
        w1_flat = self.layer1.weights.flatten()
        b1_flat = self.layer1.biases.flatten()
        w2_flat = self.layer2.weights.flatten()
        b2_flat = self.layer2.biases.flatten()
        w3_flat = self.layer3.weights.flatten()
        b3_flat = self.layer3.biases.flatten()
        return np.concatenate([w1_flat, b1_flat, w2_flat, b2_flat, w3_flat, b3_flat])

    def set_flattened_weights(self, flat_weights: np.ndarray):
        if len(flat_weights) != 1140:
            raise ValueError(f"Weight vector length must be exactly 1140, received {len(flat_weights)}")
        
        idx = 0
        w1_size = self.config.input_size * self.config.hidden1_size
        self.layer1.weights = flat_weights[idx:idx + w1_size].reshape(self.config.input_size, self.config.hidden1_size)
        idx += w1_size

        b1_size = self.config.hidden1_size
        self.layer1.biases = flat_weights[idx:idx + b1_size]
        idx += b1_size

        w2_size = self.config.hidden1_size * self.config.hidden2_size
        self.layer2.weights = flat_weights[idx:idx + w2_size].reshape(self.config.hidden1_size, self.config.hidden2_size)
        idx += w2_size

        b2_size = self.config.hidden2_size
        self.layer2.biases = flat_weights[idx:idx + b2_size]
        idx += b2_size

        w3_size = self.config.hidden2_size * self.config.output_size
        self.layer3.weights = flat_weights[idx:idx + w3_size].reshape(self.config.hidden2_size, self.config.output_size)
        idx += w3_size

        b3_size = self.config.output_size
        self.layer3.biases = flat_weights[idx:idx + b3_size]

class FeatureExtractor:
    @staticmethod
    def clamp01(v: float) -> float:
        if v < 0.0:
            return 0.0
        if v > 1.0:
            return 1.0
        return float(v)

    @classmethod
    def extract_features_from_bands(
        cls,
        delta_p: float,
        theta_p: float,
        alpha_p: float,
        beta_p: float,
        gamma_p: float,
        dominant_freq: float,
        high_freq_energy: float = 0.0,
        samples: int = 512
    ) -> np.ndarray:
        delta_p = max(0.0001, delta_p)
        theta_p = max(0.0001, theta_p)
        alpha_p = max(0.0001, alpha_p)
        beta_p = max(0.0001, beta_p)
        gamma_p = max(0.0001, gamma_p)
        high_freq_energy = max(0.0, high_freq_energy)

        total_eeg = delta_p + theta_p + alpha_p + beta_p + gamma_p
        if total_eeg < 1e-6:
            total_eeg = 1e-6

        rel_delta = delta_p / total_eeg
        rel_theta = theta_p / total_eeg
        rel_alpha = alpha_p / total_eeg
        rel_beta = beta_p / total_eeg
        rel_gamma = gamma_p / total_eeg

        muscle_wake_factor = high_freq_energy / (total_eeg + high_freq_energy + 1e-5)

        features = np.zeros(16, dtype=np.float32)
        features[0] = cls.clamp01(rel_delta)
        features[1] = cls.clamp01(rel_theta)
        features[2] = cls.clamp01(rel_alpha)
        features[3] = cls.clamp01(rel_beta + (muscle_wake_factor * 0.5))
        features[4] = cls.clamp01(rel_gamma + (muscle_wake_factor * 0.5))

        features[5] = cls.clamp01((delta_p / (theta_p + 1e-5)) / 10.0)
        features[6] = cls.clamp01((theta_p / (alpha_p + 1e-5)) / 10.0)
        features[7] = cls.clamp01((theta_p / (beta_p + 1e-5)) / 10.0)
        features[8] = cls.clamp01(((delta_p + theta_p) / (alpha_p + beta_p + 1e-5)) / 10.0)

        features[9] = cls.clamp01((dominant_freq - 0.5) / 44.5)

        entropy = -(
            rel_delta * math.log(rel_delta + 1e-5) +
            rel_theta * math.log(rel_theta + 1e-5) +
            rel_alpha * math.log(rel_alpha + 1e-5) +
            rel_beta * math.log(rel_beta + 1e-5) +
            rel_gamma * math.log(rel_gamma + 1e-5)
        )
        features[10] = cls.clamp01(entropy / 2.5)

        features[11] = cls.clamp01(math.log(total_eeg + high_freq_energy + 1.0) / math.log(10001.0))
        features[12] = cls.clamp01(rel_alpha + rel_beta + muscle_wake_factor)
        features[13] = cls.clamp01(rel_delta + rel_theta)
        features[14] = cls.clamp01(math.sqrt((total_eeg + high_freq_energy) / (samples / 2.0)) / 200.0)
        features[15] = cls.clamp01(rel_beta + rel_gamma + muscle_wake_factor)

        return features

class SleepDataLoader:
    def __init__(self, data_path: str):
        self.data_path = data_path

    def load_records_from_store(self) -> Tuple[np.ndarray, np.ndarray, Dict[str, Any]]:
        if not os.path.exists(self.data_path):
            raise FileNotFoundError(f"Sleep data store not found at {self.data_path}")

        with open(self.data_path, "r", encoding="utf-8") as f:
            raw_data = json.load(f)

        raw_logs = []
        if "sleepLogs" in raw_data and isinstance(raw_data["sleepLogs"], list):
            raw_logs.extend(raw_data["sleepLogs"])

        if "activeSession" in raw_data and raw_data["activeSession"] and "logs" in raw_data["activeSession"]:
            raw_logs.extend(raw_data["activeSession"]["logs"])

        if "completedSessions" in raw_data and isinstance(raw_data["completedSessions"], list):
            for s in raw_data["completedSessions"]:
                if "logs" in s and isinstance(s["logs"], list):
                    raw_logs.extend(s["logs"])

        feature_matrix = []
        label_vector = []

        for item in raw_logs:
            try:
                dom_freq = float(item.get("dominant_freq", 8.0))
                delta = float(item.get("delta", 2.0))
                theta = float(item.get("theta", 4.0))
                alpha = float(item.get("alpha", 8.0))
                beta = float(item.get("beta", 3.0))
                gamma = float(item.get("gamma", 1.0))

                stage_code = item.get("stage_code", None)
                if stage_code is None:
                    stage_str = str(item.get("stage", "")).upper()
                    if "WAKE" in stage_str:
                        stage_code = 0
                    elif "LIGHT" in stage_str:
                        stage_code = 1
                    elif "DEEP" in stage_str:
                        stage_code = 2
                    elif "REM" in stage_str:
                        stage_code = 3
                    else:
                        stage_code = 0
                else:
                    stage_code = int(stage_code)

                if stage_code < 0 or stage_code > 3:
                    continue

                feat = FeatureExtractor.extract_features_from_bands(
                    delta_p=delta,
                    theta_p=theta,
                    alpha_p=alpha,
                    beta_p=beta,
                    gamma_p=gamma,
                    dominant_freq=dom_freq
                )
                feature_matrix.append(feat)
                label_vector.append(stage_code)
            except Exception:
                continue

        stats = {
            "total_raw_epochs": len(raw_logs),
            "parsed_features_count": len(feature_matrix),
            "wake_count": sum(1 for l in label_vector if l == 0),
            "light_count": sum(1 for l in label_vector if l == 1),
            "deep_count": sum(1 for l in label_vector if l == 2),
            "rem_count": sum(1 for l in label_vector if l == 3)
        }

        if len(feature_matrix) == 0:
            return np.empty((0, 16), dtype=np.float32), np.empty((0,), dtype=np.int32), stats

        return np.array(feature_matrix, dtype=np.float32), np.array(label_vector, dtype=np.int32), stats

class SyntheticEEGGenerator:
    @staticmethod
    def generate_physiologic_distribution(
        stage: int,
        count: int,
        seed: int = 42
    ) -> np.ndarray:
        rng = np.random.RandomState(seed + stage * 100)
        samples = []

        for _ in range(count):
            if stage == 0:
                dom_freq = rng.uniform(8.5, 32.0)
                delta = rng.exponential(scale=3.0) + 1.0
                theta = rng.exponential(scale=4.0) + 2.0
                alpha = rng.normal(loc=18.0, scale=4.0) if dom_freq < 13.0 else rng.normal(loc=8.0, scale=2.0)
                beta = rng.normal(loc=22.0, scale=5.0)
                gamma = rng.normal(loc=12.0, scale=3.0)
                hfe = rng.exponential(scale=15.0)
            elif stage == 1:
                dom_freq = rng.uniform(4.0, 7.9)
                delta = rng.normal(loc=14.0, scale=3.0)
                theta = rng.normal(loc=35.0, scale=6.0)
                alpha = rng.normal(loc=12.0, scale=3.0)
                beta = rng.normal(loc=8.0, scale=2.0)
                gamma = rng.normal(loc=3.0, scale=1.0)
                hfe = rng.exponential(scale=2.0)
            elif stage == 2:
                dom_freq = rng.uniform(0.5, 3.8)
                delta = rng.normal(loc=85.0, scale=12.0)
                theta = rng.normal(loc=10.0, scale=2.5)
                alpha = rng.normal(loc=4.0, scale=1.0)
                beta = rng.normal(loc=2.0, scale=0.8)
                gamma = rng.normal(loc=1.0, scale=0.5)
                hfe = rng.exponential(scale=0.5)
            elif stage == 3:
                dom_freq = rng.uniform(14.0, 28.0)
                delta = rng.normal(loc=6.0, scale=1.5)
                theta = rng.normal(loc=24.0, scale=4.0)
                alpha = rng.normal(loc=10.0, scale=2.0)
                beta = rng.normal(loc=32.0, scale=6.0)
                gamma = rng.normal(loc=16.0, scale=3.5)
                hfe = rng.exponential(scale=1.0)
            else:
                dom_freq = 10.0
                delta, theta, alpha, beta, gamma, hfe = 5.0, 5.0, 5.0, 5.0, 5.0, 1.0

            delta = max(0.1, delta)
            theta = max(0.1, theta)
            alpha = max(0.1, alpha)
            beta = max(0.1, beta)
            gamma = max(0.1, gamma)
            hfe = max(0.0, hfe)

            feat = FeatureExtractor.extract_features_from_bands(
                delta_p=delta,
                theta_p=theta,
                alpha_p=alpha,
                beta_p=beta,
                gamma_p=gamma,
                dominant_freq=dom_freq,
                high_freq_energy=hfe
            )
            samples.append(feat)

        return np.array(samples, dtype=np.float32)

class DataAugmentationEngine:
    @staticmethod
    def jitter(x: np.ndarray, sigma: float = 0.02, rng: Optional[np.random.RandomState] = None) -> np.ndarray:
        if rng is None:
            rng = np.random.RandomState()
        noise = rng.normal(loc=0.0, scale=sigma, size=x.shape)
        return np.clip(x + noise, 0.0, 1.0).astype(np.float32)

    @staticmethod
    def scaling(x: np.ndarray, sigma: float = 0.05, rng: Optional[np.random.RandomState] = None) -> np.ndarray:
        if rng is None:
            rng = np.random.RandomState()
        factor = rng.normal(loc=1.0, scale=sigma, size=(x.shape[0], 1))
        return np.clip(x * factor, 0.0, 1.0).astype(np.float32)

    @staticmethod
    def mixup_batch(x: np.ndarray, y: np.ndarray, alpha: float = 0.2, rng: Optional[np.random.RandomState] = None) -> Tuple[np.ndarray, np.ndarray]:
        if rng is None:
            rng = np.random.RandomState()
        n = x.shape[0]
        indices = rng.permutation(n)
        lam = rng.beta(alpha, alpha)
        x_mixed = lam * x + (1.0 - lam) * x[indices]
        y_mixed = lam * y + (1.0 - lam) * y[indices]
        return x_mixed.astype(np.float32), y_mixed.astype(np.float32)

class ModelEvaluator:
    @staticmethod
    def compute_confusion_matrix(y_true: np.ndarray, y_pred: np.ndarray, num_classes: int = 4) -> np.ndarray:
        cm = np.zeros((num_classes, num_classes), dtype=np.int32)
        for t, p in zip(y_true, y_pred):
            if 0 <= t < num_classes and 0 <= p < num_classes:
                cm[t, p] += 1
        return cm

    @staticmethod
    def compute_classification_metrics(cm: np.ndarray) -> Dict[str, Any]:
        num_classes = cm.shape[0]
        total_samples = np.sum(cm)
        accuracy = np.trace(cm) / total_samples if total_samples > 0 else 0.0

        precisions = []
        recalls = []
        f1_scores = []
        supports = []

        for c in range(num_classes):
            tp = cm[c, c]
            fp = np.sum(cm[:, c]) - tp
            fn = np.sum(cm[c, :]) - tp
            supp = np.sum(cm[c, :])

            p = tp / (tp + fp) if (tp + fp) > 0 else 0.0
            r = tp / (tp + fn) if (tp + fn) > 0 else 0.0
            f1 = (2.0 * p * r) / (p + r) if (p + r) > 0 else 0.0

            precisions.append(p)
            recalls.append(r)
            f1_scores.append(f1)
            supports.append(supp)

        macro_f1 = float(np.mean(f1_scores))
        weighted_f1 = float(np.sum(np.array(f1_scores) * np.array(supports)) / total_samples) if total_samples > 0 else 0.0

        po = accuracy
        pe = 0.0
        for c in range(num_classes):
            row_sum = np.sum(cm[c, :])
            col_sum = np.sum(cm[:, c])
            pe += (row_sum * col_sum) / (total_samples * total_samples) if total_samples > 0 else 0.0

        cohen_kappa = (po - pe) / (1.0 - pe) if (1.0 - pe) > 0 else 0.0

        return {
            "accuracy": float(accuracy),
            "macro_f1": macro_f1,
            "weighted_f1": weighted_f1,
            "cohen_kappa": float(cohen_kappa),
            "per_class_precision": [float(v) for v in precisions],
            "per_class_recall": [float(v) for v in recalls],
            "per_class_f1": [float(v) for v in f1_scores],
            "supports": [int(v) for v in supports]
        }

    @staticmethod
    def render_ascii_confusion_matrix(cm: np.ndarray, class_names: List[str]) -> str:
        lines = []
        header = f"{'':15} | " + " | ".join([f"{name:^12}" for name in class_names]) + " | Total"
        lines.append("-" * len(header))
        lines.append(f"{'PREDICTED ->':15} | " + " | ".join([f"{name:^12}" for name in class_names]) + " |")
        lines.append(f"{'TRUE STAGE':15} | " + " | ".join(["-" * 12 for _ in class_names]) + " | -----")
        
        for i, name in enumerate(class_names):
            row_vals = [f"{cm[i, j]:^12d}" for j in range(len(class_names))]
            row_total = np.sum(cm[i, :])
            lines.append(f"{name:15} | " + " | ".join(row_vals) + f" | {row_total:>5d}")

        lines.append("-" * len(header))
        col_totals = [f"{np.sum(cm[:, j]):^12d}" for j in range(len(class_names))]
        grand_total = np.sum(cm)
        lines.append(f"{'Total Pred':15} | " + " | ".join(col_totals) + f" | {grand_total:>5d}")
        lines.append("-" * len(header))
        return "\n".join(lines)

class WeightExporter:
    @staticmethod
    def format_as_cpp_progmem(weights: np.ndarray, elements_per_line: int = 8) -> str:
        lines = []
        lines.append(f"const float DEFAULT_NN_WEIGHTS[{len(weights)}] PROGMEM = {{")
        for i in range(0, len(weights), elements_per_line):
            chunk = weights[i:i + elements_per_line]
            formatted_chunk = ", ".join([f"{val:.6f}f" for val in chunk])
            comma = "," if (i + elements_per_line) < len(weights) else ""
            lines.append(f"    {formatted_chunk}{comma}")
        lines.append("};")
        return "\n".join(lines)

    @staticmethod
    def save_cpp_header(weights: np.ndarray, filepath: str, metrics: Dict[str, Any]):
        cpp_code = WeightExporter.format_as_cpp_progmem(weights)
        header_guard = "__OPTIMIZED_RHYTHMSLEEP_WEIGHTS_H__"
        content = [
            f"#ifndef {header_guard}",
            f"#define {header_guard}",
            "",
            "#include <Arduino.h>",
            "",
            f"// Trained RhythmSleep MLP Parameters (Total: {len(weights)})",
            f"// Model Accuracy: {metrics.get('accuracy', 0.0) * 100:.2f}%",
            f"// Macro F1-Score: {metrics.get('macro_f1', 0.0) * 100:.2f}%",
            f"// Cohen's Kappa: {metrics.get('cohen_kappa', 0.0):.4f}",
            "",
            cpp_code,
            "",
            f"#endif // {header_guard}",
            ""
        ]
        with open(filepath, "w", encoding="utf-8") as f:
            f.write("\n".join(content))

    @staticmethod
    def save_json_weights(
        weights: np.ndarray,
        filepath: str,
        metrics: Dict[str, Any],
        config: NeuralNetworkArchitectureConfig
    ):
        idx = 0
        w1_size = config.input_size * config.hidden1_size
        w1 = weights[idx:idx + w1_size].reshape(config.input_size, config.hidden1_size).tolist()
        idx += w1_size

        b1_size = config.hidden1_size
        b1 = weights[idx:idx + b1_size].tolist()
        idx += b1_size

        w2_size = config.hidden1_size * config.hidden2_size
        w2 = weights[idx:idx + w2_size].reshape(config.hidden1_size, config.hidden2_size).tolist()
        idx += w2_size

        b2_size = config.hidden2_size
        b2 = weights[idx:idx + b2_size].tolist()
        idx += b2_size

        w3_size = config.hidden2_size * config.output_size
        w3 = weights[idx:idx + w3_size].reshape(config.hidden2_size, config.output_size).tolist()
        idx += w3_size

        b3_size = config.output_size
        b3 = weights[idx:idx + b3_size].tolist()

        payload = {
            "metadata": {
                "generated_at": time.strftime("%Y-%m-%d %H:%M:%S UTC", time.gmtime()),
                "total_parameters": len(weights),
                "metrics": metrics,
                "configuration": config.to_dict()
            },
            "layers": {
                "layer1": {
                    "shape": [config.input_size, config.hidden1_size],
                    "weights": w1,
                    "biases": b1
                },
                "layer2": {
                    "shape": [config.hidden1_size, config.hidden2_size],
                    "weights": w2,
                    "biases": b2
                },
                "layer3": {
                    "shape": [config.hidden2_size, config.output_size],
                    "weights": w3,
                    "biases": b3
                }
            },
            "flat_weights": weights.tolist()
        }

        with open(filepath, "w", encoding="utf-8") as f:
            json.dump(payload, f, indent=2)

class TrainingPipeline:
    def __init__(self, config: NeuralNetworkArchitectureConfig, store_path: str):
        self.config = config
        self.store_path = store_path
        self.model = RhythmSleepMultiLayerPerceptron(config)
        self.stage_names = ["WAKE", "LIGHT SLEEP", "DEEP SLEEP", "REM SLEEP"]

    def prepare_dataset(self) -> Tuple[np.ndarray, np.ndarray, np.ndarray, np.ndarray]:
        loader = SleepDataLoader(self.store_path)
        x_raw, y_raw, stats = loader.load_records_from_store()

        print("=" * 80)
        print("RHYTHMSLEEP NEURAL NETWORK OPTIMIZER & WEIGHT SYNTHESIS ENGINE")
        print("=" * 80)
        print(f"Data source loaded: {self.store_path}")
        print(f"Total raw epochs found: {stats['total_raw_epochs']}")
        print(f"Parsed valid feature vectors: {stats['parsed_features_count']}")
        print(f"Stage distribution -> Wake: {stats['wake_count']}, Light: {stats['light_count']}, Deep: {stats['deep_count']}, REM: {stats['rem_count']}")

        x_synthetic_list = []
        y_synthetic_list = []
        target_stage_samples = 400

        for stage_idx in range(4):
            gen_samples = SyntheticEEGGenerator.generate_physiologic_distribution(
                stage=stage_idx,
                count=target_stage_samples,
                seed=self.config.seed
            )
            x_synthetic_list.append(gen_samples)
            y_synthetic_list.append(np.full(target_stage_samples, stage_idx, dtype=np.int32))

        x_syn = np.vstack(x_synthetic_list)
        y_syn = np.concatenate(y_synthetic_list)

        if len(x_raw) > 0:
            x_combined = np.vstack([x_raw, x_syn])
            y_combined = np.concatenate([y_raw, y_syn])
        else:
            x_combined = x_syn
            y_combined = y_syn

        if self.config.use_augmentation:
            rng = np.random.RandomState(self.config.seed)
            aug_count = int(len(x_combined) * (self.config.augmentation_factor - 1.0))
            if aug_count > 0:
                rand_indices = rng.choice(len(x_combined), size=aug_count, replace=True)
                x_sub = x_combined[rand_indices]
                y_sub = y_combined[rand_indices]
                x_aug = DataAugmentationEngine.jitter(x_sub, sigma=0.015, rng=rng)
                x_aug = DataAugmentationEngine.scaling(x_aug, sigma=0.03, rng=rng)
                x_combined = np.vstack([x_combined, x_aug])
                y_combined = np.concatenate([y_combined, y_sub])

        rng_shuffle = np.random.RandomState(self.config.seed)
        perm = rng_shuffle.permutation(len(x_combined))
        x_all = x_combined[perm]
        y_all = y_combined[perm]

        val_ratio = 0.20
        split_idx = int(len(x_all) * (1.0 - val_ratio))
        x_train, x_val = x_all[:split_idx], x_all[split_idx:]
        y_train, y_val = y_all[:split_idx], y_all[split_idx:]

        print(f"Final training set shape: {x_train.shape}, Validation set shape: {x_val.shape}")
        print("-" * 80)
        return x_train, y_train, x_val, y_val

    def one_hot(self, y: np.ndarray, num_classes: int = 4) -> np.ndarray:
        return np.eye(num_classes, dtype=np.float32)[y]

    def train(self) -> Tuple[np.ndarray, Dict[str, Any]]:
        x_train, y_train_idx, x_val, y_val_idx = self.prepare_dataset()
        y_train_onehot = self.one_hot(y_train_idx, self.config.output_size)
        y_val_onehot = self.one_hot(y_val_idx, self.config.output_size)

        best_val_loss = float("inf")
        best_weights = self.model.get_flattened_weights()
        patience_counter = 0
        training_history = []

        num_samples = x_train.shape[0]
        batch_size = self.config.batch_size
        steps_per_epoch = max(1, num_samples // batch_size)

        print(f"Beginning backpropagation optimization across {self.config.epochs} epochs (Batch size: {batch_size})...")
        print(f"{'Epoch':^7} | {'Train Loss':^12} | {'Train Acc':^11} | {'Val Loss':^12} | {'Val Acc':^11} | {'Macro F1':^10} | {'LR':^10} | Status")
        print("-" * 90)

        for epoch in range(1, self.config.epochs + 1):
            rng = np.random.RandomState(self.config.seed + epoch)
            perm = rng.permutation(num_samples)
            x_shuffled = x_train[perm]
            y_shuffled = y_train_onehot[perm]

            epoch_train_loss = 0.0
            epoch_correct = 0

            for step in range(steps_per_epoch):
                start = step * batch_size
                end = min(start + batch_size, num_samples)
                x_batch = x_shuffled[start:end]
                y_batch = y_shuffled[start:end]

                if self.config.mixup_alpha > 0.0 and rng.uniform() < 0.5:
                    x_batch_mix, y_batch_mix = DataAugmentationEngine.mixup_batch(
                        x_batch, y_batch, alpha=self.config.mixup_alpha, rng=rng
                    )
                    y_pred = self.model.forward(x_batch_mix)
                    self.model.backward(y_pred, y_batch_mix)
                else:
                    y_pred = self.model.forward(x_batch)
                    self.model.backward(y_pred, y_batch)

                self.model.optimize_step()

                loss_val = LossFunctions.cross_entropy(y_pred, y_batch, label_smoothing=self.config.label_smoothing)
                epoch_train_loss += loss_val * (end - start)
                pred_classes = np.argmax(y_pred, axis=-1)
                true_classes = np.argmax(y_batch, axis=-1)
                epoch_correct += np.sum(pred_classes == true_classes)

            self.model.scheduler_step()

            train_loss = epoch_train_loss / num_samples
            train_acc = epoch_correct / num_samples

            val_pred = self.model.forward(x_val)
            val_loss = LossFunctions.cross_entropy(val_pred, y_val_onehot)
            val_pred_classes = np.argmax(val_pred, axis=-1)
            val_acc = np.mean(val_pred_classes == y_val_idx)

            cm_val = ModelEvaluator.compute_confusion_matrix(y_val_idx, val_pred_classes, self.config.output_size)
            metrics_val = ModelEvaluator.compute_classification_metrics(cm_val)
            macro_f1 = metrics_val["macro_f1"]

            current_lr = self.model.optimizer.learning_rate

            is_best = False
            if val_loss < best_val_loss:
                best_val_loss = val_loss
                best_weights = self.model.get_flattened_weights()
                patience_counter = 0
                is_best = True
                status_str = "★ BEST"
            else:
                patience_counter += 1
                status_str = f"[{patience_counter}/{self.config.early_stopping_patience}]"

            if epoch % 10 == 0 or epoch == 1 or is_best or epoch == self.config.epochs:
                print(f"{epoch:^7d} | {train_loss:^12.4f} | {train_acc * 100:^10.2f}% | {val_loss:^12.4f} | {val_acc * 100:^10.2f}% | {macro_f1 * 100:^9.2f}% | {current_lr:^10.6f} | {status_str}")

            training_history.append({
                "epoch": epoch,
                "train_loss": train_loss,
                "train_acc": train_acc,
                "val_loss": val_loss,
                "val_acc": val_acc,
                "macro_f1": macro_f1
            })

            if patience_counter >= self.config.early_stopping_patience:
                print("-" * 90)
                print(f"Early stopping triggered at epoch {epoch}. Restoring best model checkpoint (Val Loss: {best_val_loss:.4f}).")
                break

        self.model.set_flattened_weights(best_weights)
        final_val_pred = self.model.forward(x_val)
        final_val_classes = np.argmax(final_val_pred, axis=-1)
        final_cm = ModelEvaluator.compute_confusion_matrix(y_val_idx, final_val_classes, self.config.output_size)
        final_metrics = ModelEvaluator.compute_classification_metrics(final_cm)

        print("=" * 80)
        print("OPTIMIZATION COMPLETE - FINAL VALIDATION MATRIX & PERFORMANCE REPORT")
        print("=" * 80)
        print(ModelEvaluator.render_ascii_confusion_matrix(final_cm, self.stage_names))
        print("")
        print(f"Overall Accuracy          : {final_metrics['accuracy'] * 100:.2f}%")
        print(f"Macro F1-Score            : {final_metrics['macro_f1'] * 100:.2f}%")
        print(f"Weighted F1-Score         : {final_metrics['weighted_f1'] * 100:.2f}%")
        print(f"Cohen's Kappa Coefficient : {final_metrics['cohen_kappa']:.4f}")
        print("")
        print(f"{'Class Stage':<15} | {'Precision':<10} | {'Recall':<10} | {'F1-Score':<10} | {'Support':<8}")
        print("-" * 65)
        for idx, name in enumerate(self.stage_names):
            p = final_metrics["per_class_precision"][idx] * 100
            r = final_metrics["per_class_recall"][idx] * 100
            f = final_metrics["per_class_f1"][idx] * 100
            s = final_metrics["supports"][idx]
            print(f"{name:<15} | {p:>8.2f}%  | {r:>8.2f}%  | {f:>8.2f}%  | {s:>8d}")
        print("=" * 80)

        return best_weights, final_metrics

def main():
    repo_root = os.path.dirname(os.path.abspath(__file__))
    store_file = os.path.join(repo_root, "server", "data", "store.json")
    output_header = os.path.join(repo_root, "optimized_weights.h")
    output_json = os.path.join(repo_root, "optimized_weights.json")

    config = NeuralNetworkArchitectureConfig(
        input_size=16,
        hidden1_size=32,
        hidden2_size=16,
        output_size=4,
        learning_rate=0.005,
        weight_decay=0.00005,
        epochs=120,
        batch_size=32,
        optimizer_type="adam",
        label_smoothing=0.05,
        seed=1337,
        early_stopping_patience=20
    )

    pipeline = TrainingPipeline(config=config, store_path=store_file)
    optimized_weights, metrics = pipeline.train()

    print(f"Exporting optimized weights (1,140 floating point parameters)...")
    WeightExporter.save_cpp_header(optimized_weights, output_header, metrics)
    WeightExporter.save_json_weights(optimized_weights, output_json, metrics, config)
    print(f"Successfully saved C++ PROGMEM header to: {output_header}")
    print(f"Successfully saved JSON structure to   : {output_json}")
    print("=" * 80)
    print("NEW OPTIMIZED PROGMEM ARRAY FOR ESP32-S3 FIRMWARE:")
    print("=" * 80)
    print(WeightExporter.format_as_cpp_progmem(optimized_weights))
    print("=" * 80)

if __name__ == "__main__":
    main()
