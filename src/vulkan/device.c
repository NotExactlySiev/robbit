#include <stdio.h>
#include <vulkan/vulkan.h>

// creates a generic vulkan device
VkDevice create_vulkan_device(VkInstance inst, VkSurfaceKHR surface, VkPhysicalDevice *pdev, uint32_t *queue_family_idx)
{
    VkResult rc;
    uint32_t devcount = 0;
    VkPhysicalDeviceProperties prop;
    VkPhysicalDeviceFeatures feat;

    // what physical devices do we have?
    vkEnumeratePhysicalDevices(inst, &devcount, NULL);
    VkPhysicalDevice devices[devcount];
    vkEnumeratePhysicalDevices(inst, &devcount, devices);

    // select one appropriate for us (for now just the first one)
    for (int i = 0; i < devcount; i++) {
        vkGetPhysicalDeviceProperties(devices[i], &prop);
        vkGetPhysicalDeviceFeatures(devices[i], &feat);
        printf("\t%d\t%s\n", prop.deviceID, prop.deviceName);
        *pdev = devices[i];
        break;
    }

    uint32_t queue_family_count;
    VkQueueFlags qflags;
    VkQueueFamilyProperties *qp;

    vkGetPhysicalDeviceQueueFamilyProperties(*pdev, &queue_family_count, NULL);
    VkQueueFamilyProperties queue_family_prop[queue_family_count];
    vkGetPhysicalDeviceQueueFamilyProperties(*pdev, &queue_family_count, queue_family_prop);
    printf("Found %d Queue families\n", queue_family_count);

    printf("\t\t\t\tE D P B T C G\n");
    for (int i = 0; i < queue_family_count; i++)
    {
        qp = queue_family_prop + i;
        qflags = qp->queueFlags;
        printf("Family %d: %d queues\t%d bits\t", i, qp->queueCount, qp->timestampValidBits);
        for (int j = 0x40; j > 0; j >>= 1)
        {
            printf("%c ", qflags & j ? 'X' : ' ');
        }
        printf("\n");

        // pick a queue that can do graphics
        if (qflags & VK_QUEUE_GRAPHICS_BIT) 
            *queue_family_idx = i;
    }

    VkBool32 supported;
    vkGetPhysicalDeviceSurfaceSupportKHR(*pdev, *queue_family_idx, surface, &supported);
    
    if (supported == VK_FALSE)
        printf("The selected queue family doesn't support presentation.\n");

    // we found the queue family we need, now let's define a logical device
    VkDevice ldev;
    VkPhysicalDeviceFeatures ldev_features = {};

    const float qpriorities[1] = { 1.0 };

    VkDeviceQueueCreateInfo queuec = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .flags = 0,
        .queueFamilyIndex = *queue_family_idx,
        .queueCount = 1,
        .pQueuePriorities = qpriorities,
    };

    // ASSUMES: we want these exact extensions and nothing else
    const char *extensions[2] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME, 
                                  VK_KHR_MAINTENANCE_5_EXTENSION_NAME };

    VkDeviceCreateInfo ldevc = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queuec,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = extensions,
        .pEnabledFeatures = &ldev_features,
    };

    rc = vkCreateDevice(*pdev, &ldevc, NULL, &ldev);
    if (VK_SUCCESS != rc)
        printf("can't logical device %d\n", rc);

    return ldev;
}

